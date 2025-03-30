/**
 * This is the main Rust file which will start the Microkit process. Its main purpose
 * is to scrape the provided .system XML file and dynamically link into the compiled
 * C shared object binary (libmicrokit.so) to execute the necessary functions.
 */

use std::error::Error;
use std::ffi::CString;
use libloading::{Library, Symbol};
use roxmltree::Document;

type CreateSharedMemory = extern "C" fn(*const libc::c_char, libc::c_int);
type CreateProcess = extern "C" fn(*const libc::c_char);
type AddSharedMemory = extern "C" fn(*const libc::c_char, *const libc::c_char, *const libc::c_char);
type CreateChannel = extern "C" fn(*const libc::c_char, *const libc::c_char, libc::c_int);
type RunProcess = extern "C" fn(*const libc::c_char, *const libc::c_char);

pub struct Loader<'a> {
    create_shared_memory: Symbol<'a, CreateSharedMemory>,
    create_process: Symbol<'a, CreateProcess>,
    add_shared_memory: Symbol<'a, AddSharedMemory>,
    create_channel: Symbol<'a, CreateChannel>,
    run_process: Symbol<'a, RunProcess>,
}

impl<'a> Loader<'a> {
    pub fn new(lib: &'a Library) -> Result<Self, Box<dyn Error>> {
        Ok(Self {
            create_shared_memory: unsafe { lib.get(b"create_shared_memory\0")? },
            create_process: unsafe { lib.get(b"create_process\0")? },
            add_shared_memory: unsafe { lib.get(b"add_shared_memory\0")? },
            create_channel: unsafe { lib.get(b"create_channel\0")? },
            run_process: unsafe { lib.get(b"run_process\0")? },
        })
    }

    pub fn create_shared_memory(&self, name: &str, size: i32) -> Result<(), Box<dyn Error>> {
        (self.create_shared_memory)(CString::new(name)?.as_ptr(), size);
        Ok(())
    }

    pub fn create_process(&self, name: &str) -> Result<(), Box<dyn Error>> {
        (self.create_process)(CString::new(name)?.as_ptr());
        Ok(())
    }

    pub fn add_shared_memory(&self, pd_name: &str, mr_name: &str, varname: &str) -> Result<(), Box<dyn Error>> {
        (self.add_shared_memory)(CString::new(pd_name)?.as_ptr(), CString::new(mr_name)?.as_ptr(), CString::new(varname)?.as_ptr());
        Ok(())
    }

    pub fn create_channel(&self, pd1: &str, pd2: &str, id: i32) -> Result<(), Box<dyn Error>> {
        (self.create_channel)(CString::new(pd1)?.as_ptr(), CString::new(pd2)?.as_ptr(), id);
        Ok(())
    }

    pub fn run_process(&self, pd_name: &str, image_path: &str) -> Result<(), Box<dyn Error>> {
        (self.run_process)(CString::new(pd_name)?.as_ptr(), CString::new(image_path)?.as_ptr());
        Ok(())
    }
}

/* --- Find all memory regions and call the C function `create_shared_memory` for each region --- */
fn process_memory_regions(doc: &Document, loader: &Loader) -> Result<(), Box<dyn Error>> {
    for node in doc.descendants().filter(|n| n.has_tag_name("memory_region")) {
        let region_name = node.attribute("name").ok_or("Missing attribute 'name' on memory_region")?;
        let size_str = node.attribute("size").ok_or("Missing attribute 'size' on memory_region")?;
        let region_size = i32::from_str_radix(size_str.trim_start_matches("0x"), 16)?;
        loader.create_shared_memory(region_name, region_size)?;
    }
    Ok(())
}

/* --- Find all protection domains and call the necessary C functions to create them --- */
fn process_protection_domains(doc: &Document, loader: &Loader) -> Result<Vec<(String, String)>, Box<dyn Error>> {
    let mut process_list: Vec<(String, String)> = Vec::new();
    for pd in doc.descendants().filter(|n| n.has_tag_name("protection_domain")) {
        let pd_name_str = pd.attribute("name").ok_or("Missing attribute 'name' on protection_domain")?;
        loader.create_process(pd_name_str)?;

        if let Some(pd_map) = pd.descendants().find(|n| n.has_tag_name("map")) {
            let pd_map_name_str = pd_map.attribute("mr").ok_or("Missing attribute 'mr' on map")?;
            let pd_map_varname_str = pd_map.attribute("setvar_vaddr").ok_or("Missing attribute 'setvar_vaddr' on map")?;
            loader.add_shared_memory(pd_name_str, pd_map_name_str, pd_map_varname_str)?;
        }

        if let Some(pd_image) = pd.descendants().find(|n| n.has_tag_name("program_image")) {
            let pd_image_path_raw = pd_image.attribute("path").ok_or("Missing attribute 'path' on program_image")?;
            let mut pd_image_path = String::from("./build/");
            pd_image_path.push_str(&pd_image_path_raw[..pd_image_path_raw.len() - 3]);
            pd_image_path.push_str("so"); // replace .elf with .so
            process_list.push((pd_name_str.to_string(), pd_image_path));
        }
    }
    Ok(process_list)
}

/* --- Find all communication channels between the processes and create them --- */
fn process_channels(doc: &Document, loader: &Loader) -> Result<(), Box<dyn Error>> {
    for channel in doc.descendants().filter(|n| n.has_tag_name("channel")) {
        let mut ends_iter = channel.children().filter(|n| n.has_tag_name("end"));
        
        if let (Some(end1), Some(end2)) = (ends_iter.next(), ends_iter.next()) {
            let pd1 = end1.attribute("pd").ok_or("Missing attribute 'pd' on first channel end")?;
            let pd2 = end2.attribute("pd").ok_or("Missing attribute 'pd' on second channel end")?;

            let id1 = end1.attribute("id").ok_or("Missing attribute 'id' on first channel end")?.parse::<i32>()?;
            let id2 = end2.attribute("id").ok_or("Missing attribute 'id' on second channel end")?.parse::<i32>()?;
            
            loader.create_channel(pd1, pd2, id1)?;
            loader.create_channel(pd2, pd1, id2)?;
        } else {
            return Err("Expected exactly two ends for each channel".into());
        }
    }
    Ok(())
}

/* --- Start all the processes within the process list --- */
fn run_processes(process_list: Vec<(String, String)>, loader: &Loader) -> Result<(), Box<dyn Error>> {
    for (c_pd_name, c_pd_image_path) in process_list {
        loader.run_process(&c_pd_name, &c_pd_image_path)?;
    }
    Ok(())
}

fn main() -> Result<(), Box<dyn Error>> {
    /* -- Grab the C binary we will be dynamically linking into as well as the .system XML file we are parsing --- */
    let lib: Library = unsafe { Library::new("./build/libmicrokit.so")? };
    let loader: Loader<'_> = Loader::new(&lib)?;

    let xml_content: String = std::fs::read_to_string("./example/example.system")?;
    let doc: Document<'_> = roxmltree::Document::parse(&xml_content)?;

    process_memory_regions(&doc, &loader)?;
    let process_list: Vec<(String, String)> = process_protection_domains(&doc, &loader)?;
    process_channels(&doc, &loader)?;
    run_processes(process_list, &loader)?;

    std::thread::park();
    Ok(())
}
