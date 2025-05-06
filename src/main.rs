/**
 * This is the main Rust file which will start the Microkit process. Its main purpose
 * is to scrape the provided .system XML file and dynamically link into the compiled
 * C shared object binary (libmicrokit.so) to execute the necessary functions.
 */

use std::error::Error;
use std::ffi::CString;
use libloading::{Library, Symbol};
use roxmltree::Document;

type CreateProcess = extern "C" fn(*const libc::c_char, libc::c_uint);
type CreateSharedMemory = extern "C" fn(*const libc::c_char, libc::c_ulong);
type AddSharedMemory = extern "C" fn(*const libc::c_char, *const libc::c_char, *const libc::c_char);
type CreateChannel = extern "C" fn(*const libc::c_char, *const libc::c_char, libc::c_uint);
type RunProcess = extern "C" fn(*const libc::c_char, *const libc::c_char);

const KIBIBYTE: u32 = 1024;
const MEBIBYTE: u32 = KIBIBYTE * KIBIBYTE;

struct Loader<'a> {
    create_shared_memory: Symbol<'a, CreateSharedMemory>,
    create_process: Symbol<'a, CreateProcess>,
    add_shared_memory: Symbol<'a, AddSharedMemory>,
    create_channel: Symbol<'a, CreateChannel>,
    run_process: Symbol<'a, RunProcess>,

    // Keep all CString allocations alive for the lifetime of Loader
    owned_c_strings: Vec<CString>,
}

impl<'a> Loader<'a> {
    fn new(lib: &'a Library) -> Self {
        Self {
            create_shared_memory: Self::sym::<CreateSharedMemory>(lib, b"create_shared_memory"),
            create_process:       Self::sym::<CreateProcess>(lib, b"create_process"),
            add_shared_memory:    Self::sym::<AddSharedMemory>(lib, b"add_shared_memory"),
            create_channel:       Self::sym::<CreateChannel>(lib, b"create_channel"),
            run_process:          Self::sym::<RunProcess>(lib, b"run_process"),
            owned_c_strings:      Vec::new(),
        }
    }

    fn sym<T>(lib: &'a Library, name: &[u8]) -> Symbol<'a, T> {
        unsafe { lib.get(name).expect(&format!("Could not find symbol {:?}", name)) }
    }

    fn store_str(&mut self, s: &str) -> *const libc::c_char {
        let cstr = CString::new(s).expect("CString::new failed");
        let cptr = cstr.as_ptr();
        self.owned_c_strings.push(cstr);
        cptr
    }

    fn create_shared_memory(&mut self, name: &str, size: u64) {
        let name_ptr = self.store_str(name);
        (self.create_shared_memory)(name_ptr, size);
    }

    fn create_process(&mut self, name: &str, stack_size: u32) {
        let name_ptr = self.store_str(name);
        (self.create_process)(name_ptr, stack_size);
    }

    fn add_shared_memory(&mut self, pd_name: &str, mr_name: &str, varname: &str) {
        let pd_ptr   = CString::new(pd_name).expect("CString::new failed");
        let mr_ptr   = self.store_str(mr_name);
        let var_ptr  = self.store_str(varname);
        (self.add_shared_memory)(pd_ptr.as_ptr(), mr_ptr, var_ptr);
    }

    fn create_channel(&mut self, pd1: &str, pd2: &str, id: u32) {
        let pd1_ptr = CString::new(pd1).expect("CString::new failed");
        let pd2_ptr = CString::new(pd2).expect("CString::new failed");
        (self.create_channel)(pd1_ptr.as_ptr(), pd2_ptr.as_ptr(), id);
    }

    fn run_process(&mut self, pd_name: &str, image_path: &str) {
        let pd_name_ptr = CString::new(pd_name).expect("CString::new failed");
        let image_path_ptr = CString::new(image_path).expect("CString::new failed");
        (self.run_process)(pd_name_ptr.as_ptr(), image_path_ptr.as_ptr());
    }
}

/* --- Find all memory regions and call the C function `create_shared_memory` for each region --- */
fn process_memory_regions(doc: &Document, loader: &mut Loader) -> Result<(), Box<dyn Error>> {
    for node in doc.descendants().filter(|n| n.has_tag_name("memory_region")) {
        let region_name = node.attribute("name").expect("Missing attribute 'name' on memory_region");
        let size_str = node.attribute("size").expect("Missing attribute 'size' on memory_region");
        let region_size = u64::from_str_radix(size_str.trim_start_matches("0x"), 16)?;
        loader.create_shared_memory(region_name, region_size);
    }
    Ok(())
}

/* --- Find all protection domains and call the necessary C functions to create them --- */
fn process_protection_domains(doc: &Document, loader: &mut Loader) -> Result<Vec<(String, String)>, Box<dyn Error>> {
    let mut process_list: Vec<(String, String)> = Vec::new();
    for pd in doc.descendants().filter(|n| n.has_tag_name("protection_domain")) {
        let pd_name_str = pd.attribute("name").expect("Missing attribute 'name' on protection_domain");

        let stack_size_str = pd.attribute("stack_size").unwrap_or("0x1000");
        let stack_size = u32::from_str_radix(stack_size_str.trim_start_matches("0x"), 16)?;
        if stack_size < 4 * KIBIBYTE || stack_size > 16 * MEBIBYTE {
            return Err("Stack size must be between 4 KiB and 16 MiB".into());
        }

        loader.create_process(pd_name_str, stack_size);

        if let Some(pd_image) = pd.descendants().find(|n| n.has_tag_name("program_image")) {
            let pd_image_path_raw = pd_image.attribute("path").expect("Missing attribute 'path' on program_image");
            let mut pd_image_path = String::from("./build/");
            pd_image_path.push_str(&pd_image_path_raw[..pd_image_path_raw.len() - 3]);
            pd_image_path.push_str("so"); // replace .elf with .so
            process_list.push((pd_name_str.to_string(), pd_image_path));
        }

        if let Some(pd_map) = pd.descendants().find(|n| n.has_tag_name("map")) {
            let pd_map_name_str = pd_map.attribute("mr").expect("Missing attribute 'mr' on map");
            let pd_map_varname_str = pd_map.attribute("setvar_vaddr").expect("Missing attribute 'setvar_vaddr' on map");
            loader.add_shared_memory(pd_name_str, pd_map_name_str, pd_map_varname_str);
        }
    }
    Ok(process_list)
}

/* --- Find all communication channels between the processes and create them --- */
fn process_channels(doc: &Document, loader: &mut Loader) -> Result<(), Box<dyn Error>> {
    for channel in doc.descendants().filter(|n| n.has_tag_name("channel")) {
        let mut ends_iter = channel.children().filter(|n| n.has_tag_name("end"));
        
        if let (Some(end1), Some(end2)) = (ends_iter.next(), ends_iter.next()) {
            let pd1 = end1.attribute("pd").expect("Missing attribute 'pd' on first channel end");
            let pd2 = end2.attribute("pd").expect("Missing attribute 'pd' on second channel end");

            let id1 = end1.attribute("id").expect("Missing attribute 'id' on first channel end").parse()?;
            let id2 = end2.attribute("id").expect("Missing attribute 'id' on second channel end").parse()?;
            
            loader.create_channel(pd1, pd2, id1);
            loader.create_channel(pd2, pd1, id2);
        } else {
            return Err("Expected exactly two ends for each channel".into());
        }
    }
    Ok(())
}

/* --- Start all the processes within the process list --- */
fn run_processes(process_list: Vec<(String, String)>, loader: &mut Loader) -> Result<(), Box<dyn Error>> {
    for (c_pd_name, c_pd_image_path) in process_list {
        loader.run_process(&c_pd_name, &c_pd_image_path);
    }
    Ok(())
}

fn main() -> Result<(), Box<dyn Error>> {
    /* -- Grab the C binary we will be dynamically linking into as well as the .system XML file we are parsing --- */
    let lib: Library = unsafe { Library::new("./build/libmicrokit.so")? };
    let mut loader: Loader<'_> = Loader::new(&lib);

    let xml_content: String = std::fs::read_to_string("./example/example.system")?;
    let doc: Document<'_> = roxmltree::Document::parse(&xml_content)?;

    process_memory_regions(&doc, &mut loader)?;
    let process_list: Vec<(String, String)> = process_protection_domains(&doc, &mut loader)?;
    process_channels(&doc, &mut loader)?;
    run_processes(process_list, &mut loader)?;

    std::thread::park();
    Ok(())
}
