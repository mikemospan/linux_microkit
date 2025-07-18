/**
 * This is the main Rust file which will start the Microkit process. Its main purpose
 * is to scrape the provided .system XML file and dynamically link into the compiled
 * C shared object binary (libmicrokit.so) to execute the necessary functions.
 * 
 * Author: Michael Mospan (@mmospan)
 */

use std::env;
use std::error::Error;
use roxmltree::Document;
use loader_api::Loader;

const KIBIBYTE: u32 = 1024;
const MEBIBYTE: u32 = KIBIBYTE * KIBIBYTE;
const PAGE_SIZE: u32 = 4 * KIBIBYTE;

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
    let mut loader: Loader<> = Loader::new();
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