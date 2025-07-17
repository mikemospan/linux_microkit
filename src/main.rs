/**
 * This is the main Rust file which will start the Microkit process. Its main purpose
 * is to scrape the provided .system XML file and dynamically link into the compiled
 * C shared object binary (libmicrokit.so) to execute the necessary functions.
 * 
 * Author: Michael Mospan (@mmospan)
 */

use std::env;
use std::error::Error;
use std::ffi::CString;
use std::collections::HashMap;
use libloading::{Library, Symbol};
use roxmltree::Document;

// Opaque pointer to C process_t struct
type ProcessHandle = *mut libc::c_void;
type SharedMemoryHandle = *mut libc::c_void;

type CreateProcess = extern "C" fn(*const libc::c_char, libc::c_uint) -> ProcessHandle;
type CreateSharedMemory = extern "C" fn(*const libc::c_char, libc::c_ulong) -> *mut libc::c_void;
type AddSharedMemory = extern "C" fn(ProcessHandle, *mut libc::c_void, *const libc::c_char);
type CreateChannel = extern "C" fn(ProcessHandle, ProcessHandle, libc::c_ulong);
type RunProcess = extern "C" fn(ProcessHandle, *mut libc::c_char);

const KIBIBYTE: u32 = 1024;
const MEBIBYTE: u32 = KIBIBYTE * KIBIBYTE;
const PAGE_SIZE: u32 = 4 * KIBIBYTE;

struct ProcessInfo {
    handle: ProcessHandle,
    image_path: String,
}

struct Loader<'a> {
    create_shared_memory: Symbol<'a, CreateSharedMemory>,
    create_process: Symbol<'a, CreateProcess>,
    add_shared_memory: Symbol<'a, AddSharedMemory>,
    create_channel: Symbol<'a, CreateChannel>,
    run_process: Symbol<'a, RunProcess>,
    
    // Rust maintains the mappings during setup
    processes: HashMap<String, ProcessInfo>,
    shared_memory: HashMap<String, SharedMemoryHandle>,
}

impl<'a> Loader<'a> {
    fn new(lib: &'a Library) -> Self {
        Self {
            create_shared_memory: Self::sym::<CreateSharedMemory>(lib, b"create_shared_memory"),
            create_process:       Self::sym::<CreateProcess>(lib, b"create_process"),
            add_shared_memory:    Self::sym::<AddSharedMemory>(lib, b"add_shared_memory"),
            create_channel:       Self::sym::<CreateChannel>(lib, b"create_channel"),
            run_process:          Self::sym::<RunProcess>(lib, b"run_process"),
            processes:            HashMap::new(),
            shared_memory:        HashMap::new(),
        }
    }

    fn sym<T>(lib: &'a Library, name: &[u8]) -> Symbol<'a, T> {
        unsafe {
            lib.get(name).unwrap_or_else(|_| {
                panic!("Failed to load symbol '{}': symbol not found in the shared library", String::from_utf8_lossy(name))
            })
        }
    }

    fn create_shared_memory(&mut self, name: &str, size: u64) {
        let name_c = CString::new(name)
            .unwrap_or_else(|_| panic!("Shared memory name {:?} contains an internal null byte", name));
        
        let handle = (self.create_shared_memory)(name_c.as_ptr(), size);
        
        self.shared_memory.insert(name.to_string(), handle);
    }

    fn create_process(&mut self, name: &str, stack_size: u32) -> ProcessHandle {
        let name_c = CString::new(name)
            .unwrap_or_else(|_| panic!("Process name {:?} contains an internal null byte", name));
        
        let handle = (self.create_process)(name_c.as_ptr(), stack_size);
        
        self.processes.insert(name.to_string(), ProcessInfo {
            handle,
            image_path: String::new(), // Will be set later
        });
        
        handle
    }

    fn set_process_image(&mut self, pd_name: &str, image_path: String) {
        if let Some(process) = self.processes.get_mut(pd_name) {
            process.image_path = image_path;
        }
    }

    fn add_shared_memory(&mut self, pd_name: &str, mr_name: &str, varname: &str) {
        let process_handle = self.processes.get(pd_name)
            .unwrap_or_else(|| panic!("Process {} not found", pd_name))
            .handle;
        
        let shm_handle = self.shared_memory.get(mr_name)
            .unwrap_or_else(|| panic!("Shared memory {} not found", mr_name));
        
        let var_ptr = CString::new(varname)
            .unwrap_or_else(|_| panic!("Variable name {:?} contains an internal null byte", varname)).into_raw();
        
        (self.add_shared_memory)(process_handle, *shm_handle, var_ptr);
    }

    fn create_channel(&mut self, pd1: &str, pd2: &str, id: u64) {
        let process1_handle = self.processes.get(pd1)
            .unwrap_or_else(|| panic!("Process {} not found", pd1))
            .handle;
        
        let process2_handle = self.processes.get(pd2)
            .unwrap_or_else(|| panic!("Process {} not found", pd2))
            .handle;
        
        (self.create_channel)(process1_handle, process2_handle, id);
    }

    fn run_process(&mut self, pd_name: &str) {
        let process = self.processes.get(pd_name)
            .unwrap_or_else(|| panic!("Process {} not found", pd_name));
        
        let image_path_ptr = CString::new(process.image_path.as_str())
            .unwrap_or_else(|_| panic!("Image path {:?} contains an internal null byte", process.image_path)).into_raw();
        
        (self.run_process)(process.handle, image_path_ptr);
    }

    fn run_all_processes(&mut self) {
        // Clone the keys to avoid borrowing issues
        let process_names: Vec<String> = self.processes.keys().cloned().collect();
        
        for process_name in process_names {
            self.run_process(&process_name);
        }
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
fn process_protection_domains(doc: &Document, loader: &mut Loader) -> Result<(), Box<dyn Error>> {
    for pd in doc.descendants().filter(|n| n.has_tag_name("protection_domain")) {
        let pd_name_str = pd.attribute("name").expect("Missing attribute 'name' on protection_domain");
        let stack_size_str = pd.attribute("stack_size").unwrap_or("0x1000");
        let stack_size = u32::from_str_radix(stack_size_str.trim_start_matches("0x"), 16)?;
        
        if stack_size < 4 * KIBIBYTE || stack_size > 16 * MEBIBYTE {
            return Err("Stack size must be between 4 KiB and 16 MiB".into());
        }
        
        // We add an extra page size to every protection domain because glibc likes to use a lot of memory!
        loader.create_process(pd_name_str, stack_size + PAGE_SIZE);

        if let Some(pd_image) = pd.descendants().find(|n| n.has_tag_name("program_image")) {
            let pd_image_path_raw = pd_image.attribute("path").expect("Missing attribute 'path' on program_image");
            let mut pd_image_path = String::from("./build/");
            pd_image_path.push_str(&pd_image_path_raw[..pd_image_path_raw.len() - 3]);
            pd_image_path.push_str("so"); // replace .elf with .so
            loader.set_process_image(pd_name_str, pd_image_path);
        }

        if let Some(pd_map) = pd.descendants().find(|n| n.has_tag_name("map")) {
            let pd_map_name_str = pd_map.attribute("mr").expect("Missing attribute 'mr' on map");
            let pd_map_varname_str = pd_map.attribute("setvar_vaddr").expect("Missing attribute 'setvar_vaddr' on map");
            loader.add_shared_memory(pd_name_str, pd_map_name_str, pd_map_varname_str);
        }
    }
    Ok(())
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

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <config.system>", args[0]);
        std::process::exit(1);
    }

    /* -- Grab the C binary we will be dynamically linking into as well as the .system XML file we are parsing --- */
    let lib: Library = unsafe { Library::new("./build/libmicrokit.so")? };
    let mut loader: Loader<'_> = Loader::new(&lib);
    let xml_content: String = std::fs::read_to_string(format!("./example/{}", &args[1]))?;
    let doc: Document<'_> = roxmltree::Document::parse(&xml_content)?;

    // Process all components
    process_memory_regions(&doc, &mut loader)?;
    process_protection_domains(&doc, &mut loader)?;
    process_channels(&doc, &mut loader)?;
    
    // Run all processes
    loader.run_all_processes();
    
    // The loader and its hashmaps are automatically cleaned up here when they go out of scope
    std::thread::park();
    Ok(())
}