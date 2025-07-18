use std::ffi::CString;
use std::collections::HashMap;
use std::os::raw::{c_char, c_int, c_void};

unsafe extern "C" {
    fn create_shared_memory(name: *const libc::c_char, size: libc::c_ulong) -> *mut libc::c_void;
    fn create_process(name: *const libc::c_char, stack_size: libc::c_uint) -> *mut libc::c_void;
    fn add_shared_memory(process: *mut libc::c_void, memory: *mut libc::c_void, varname: *const libc::c_char);
    fn create_channel(process1: *mut libc::c_void, process2: *mut libc::c_void, id: libc::c_ulong);
    fn run_process(process: *mut libc::c_void, image_path: *mut libc::c_char);
    fn get_channel_target(from: ProcessHandle, ch: u64) -> ProcessHandle;
}

pub type ProcessHandle = *mut libc::c_void;
pub type SharedMemoryHandle = *mut libc::c_void;


#[repr(C)]
pub struct SharedMemory {
    pub shared_buffer: *mut c_void,
    pub size: u64,
}

#[repr(C)]
pub struct SharedMemoryStackNode {
    pub shm: *mut c_void,
    pub _varname: *const c_char,
    pub next: *mut SharedMemoryStackNode,
}

#[repr(C)]
pub struct Process {
    pub _path:                 *mut c_char,
    pub stack_top:             *mut c_char,
    pub sig_handler_stack:     *mut c_char,
    pub shared_memory:         *mut SharedMemoryStackNode,
    pub channel_id_to_process: *mut c_void,
    pub notification:          c_int,
    pub send_pipe:             [c_int; 2],
    pub receive_pipe:          [c_int; 2],
    pub ipc_buffer:            *mut c_void,
}

pub struct ProcessInfo {
    pub handle: ProcessHandle,
    pub image_path: String,
}

pub struct Loader<> {
    // Rust maintains the mappings during setup
    pub processes: HashMap<String, ProcessInfo>,
    pub shared_memory: HashMap<String, SharedMemoryHandle>,
}

impl<> Loader<> {
    pub fn new() -> Self {
        Self {
            processes:            HashMap::new(),
            shared_memory:        HashMap::new(),
        }
    }

    pub fn create_shared_memory(&mut self, name: &str, size: u64) {
        let name_c = CString::new(name)
            .unwrap_or_else(|_| panic!("Shared memory name {:?} contains an internal null byte", name));
        
        let handle = unsafe { create_shared_memory(name_c.as_ptr(), size) };
        
        self.shared_memory.insert(name.to_string(), handle);
    }

    pub fn create_process(&mut self, name: &str, stack_size: u32) -> ProcessHandle {
        let name_c = CString::new(name)
            .unwrap_or_else(|_| panic!("Process name {:?} contains an internal null byte", name));
        
        let handle = unsafe { create_process(name_c.as_ptr(), stack_size) };
        
        self.processes.insert(name.to_string(), ProcessInfo {
            handle,
            image_path: String::new(), // Will be set later
        });
        
        handle
    }

    pub fn set_process_image(&mut self, pd_name: &str, image_path: String) {
        if let Some(process) = self.processes.get_mut(pd_name) {
            process.image_path = image_path;
        }
    }

    pub fn add_shared_memory(&mut self, pd_name: &str, mr_name: &str, varname: &str) {
        let process_handle = self.processes.get(pd_name)
            .unwrap_or_else(|| panic!("Process {} not found", pd_name))
            .handle;
        
        let shm_handle = self.shared_memory.get(mr_name)
            .unwrap_or_else(|| panic!("Shared memory {} not found", mr_name));
        
        let var_ptr = CString::new(varname)
            .unwrap_or_else(|_| panic!("Variable name {:?} contains an internal null byte", varname)).into_raw();
        
        unsafe { add_shared_memory(process_handle, *shm_handle, var_ptr); }
    }

    pub fn create_channel(&mut self, pd1: &str, pd2: &str, id: u64) {
        let process1_handle = self.processes.get(pd1)
            .unwrap_or_else(|| panic!("Process {} not found", pd1))
            .handle;
        
        let process2_handle = self.processes.get(pd2)
            .unwrap_or_else(|| panic!("Process {} not found", pd2))
            .handle;
        
        unsafe { create_channel(process1_handle, process2_handle, id); }
    }

    pub fn run_process(&mut self, pd_name: &str) {
        let process = self.processes.get(pd_name)
            .unwrap_or_else(|| panic!("Process {} not found", pd_name));
        
        let image_path_ptr = CString::new(process.image_path.as_str())
            .unwrap_or_else(|_| panic!("Image path {:?} contains an internal null byte", process.image_path)).into_raw();
        
        unsafe { run_process(process.handle, image_path_ptr); }
    }

    pub fn run_all_processes(&mut self) {
        // Clone the keys to avoid borrowing issues
        let process_names: Vec<String> = self.processes.keys().cloned().collect();
        
        for process_name in process_names {
            self.run_process(&process_name);
        }
    }

    // Used purely for testing purposes
    pub fn get_channel_target(&self, from_process: &str, channel_id: u64) -> Option<ProcessHandle> {
        let from = self.processes.get(from_process)?.handle;
        let target = unsafe { get_channel_target(from, channel_id) };
        if target.is_null() {
            None
        } else {
            Some(target)
        }
    }
}