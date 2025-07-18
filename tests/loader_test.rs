use loader_api::*;
use std::os::raw::{c_int, c_void};

/* --- HELPER FUNCTIONS --- */

fn get_process_fields(proc: ProcessHandle) -> (bool, bool, bool, bool, c_int, [c_int; 2], [c_int; 2], bool) {
    unsafe {
        let p = &*(proc as *const Process);
        (
            !p.stack_top.is_null(),
            !p.sig_handler_stack.is_null(),
            p.shared_memory.is_null(),
            !p.channel_id_to_process.is_null(),
            p.notification,
            p.send_pipe,
            p.receive_pipe,
            !p.ipc_buffer.is_null(),
        )
    }
}

fn get_shared_memory_fields(ptr: SharedMemoryHandle) -> (bool, u64) {
    unsafe {
        let shm = &*(ptr as *const SharedMemory);
        (!shm.shared_buffer.is_null(), shm.size)
    }
}

fn get_stack_node_fields(ptr: *mut SharedMemoryStackNode) -> (*mut c_void, bool, *mut SharedMemoryStackNode) {
    unsafe {
        let node = &*ptr;
        (node.shm, !node._varname.is_null(), node.next)
    }
}

/* --- TEST EXTENSIONS --- */

trait LoaderTestExt {
    fn get_shared_memory_handle(&self, name: &str) -> Option<SharedMemoryHandle>;
}

impl LoaderTestExt for Loader {
    fn get_shared_memory_handle(&self, name: &str) -> Option<SharedMemoryHandle> {
        self.shared_memory.get(name).copied()
    }
}

/* --- TESTS --- */

#[test]
fn test_process_creation() {
    let mut loader = Loader::new();
    let proc = loader.create_process("test_proc", 0x1000);
    assert!(!proc.is_null());

    let (stack_top, sig_stack, shm_null, channel_ok, notif, send, recv, ipc_ok) = get_process_fields(proc);
    assert!(stack_top, "Stack top should not be null");
    assert!(sig_stack, "Signal handler stack should not be null");
    assert!(ipc_ok, "IPC buffer should be mapped");
    assert!(shm_null, "Shared memory list should initially be null");
    assert!(channel_ok, "Channel map should be allocated");
    assert!(notif >= 0, "Notification fd should be valid");

    assert_ne!(send[0], send[1], "Send pipe ends should differ");
    assert_ne!(recv[0], recv[1], "Receive pipe ends should differ");

    let fds = [send[0], send[1], recv[0], recv[1], notif];
    for i in 0..fds.len() {
        for j in (i + 1)..fds.len() {
            assert_ne!(fds[i], fds[j], "Duplicate fds found: {} and {}", fds[i], fds[j]);
        }
    }

    let process = unsafe { &*(proc as *const Process) };
    assert_eq!(process.stack_top as usize % 16, 0, "Stack should be 16-byte aligned");
    assert_eq!(process.sig_handler_stack as usize % 16, 0, "Signal stack should be 16-byte aligned");
}

#[test]
fn test_shared_memory_creation() {
    let mut loader = Loader::new();
    loader.create_shared_memory("test_shm", 0x1000);
    let shm = loader.get_shared_memory_handle("test_shm").unwrap();
    assert!(!shm.is_null(), "Shared memory handle should not be null");

    let (buffer_valid, size) = get_shared_memory_fields(shm);
    assert_eq!(size, 0x1000, "Expected shared memory size 0x1000");
    assert!(buffer_valid, "Shared buffer pointer should not be null");

    let shm_struct = unsafe { &*(shm as *const SharedMemory) };
    assert_eq!(shm_struct.shared_buffer as usize % 4096, 0, "Shared buffer should be page aligned");

    unsafe {
        let buf = shm_struct.shared_buffer as *mut u8;
        *buf = 42;
        assert_eq!(*buf, 42, "Able to write to shared memory buffer");
    }
}

#[test]
fn test_add_shared_memory() {
    let mut loader = Loader::new();
    let proc = loader.create_process("test_proc", 0x1000);
    loader.create_shared_memory("test_shm", 0x2000);
    loader.add_shared_memory("test_proc", "test_shm", "var1");

    let proc_ptr = proc as *const Process;
    let shm_stack = unsafe { (*proc_ptr).shared_memory };
    assert!(!shm_stack.is_null(), "Shared memory stack should not be null");

    let (first_shm, varname, next) = get_stack_node_fields(shm_stack);
    assert!(varname, "Varname for first node should not be null");
    assert!(next.is_null(), "First node should be the only node initially");

    loader.create_shared_memory("test_shm2", 0x3000);
    loader.add_shared_memory("test_proc", "test_shm2", "var2");

    let new_head = unsafe { (*proc_ptr).shared_memory };
    assert_ne!(new_head, shm_stack, "New head should differ from previous stack");

    let (second_shm, varname2, next2) = get_stack_node_fields(new_head);
    assert!(varname2, "Varname for second node should not be null");
    assert_eq!(next2, shm_stack, "Second node should point to first");

    let (old_shm, _, next_old) = get_stack_node_fields(next2);
    assert_eq!(next_old, std::ptr::null_mut(), "Tail of stack should be null");
    assert_eq!(first_shm, old_shm, "Original memory block should be unchanged");
    assert_ne!(second_shm, first_shm, "Each stack node should refer to a different memory block");
}

#[test]
fn test_channel_creation() {
    let mut loader = Loader::new();
    loader.create_process("sender", 0x1000);
    let p2 = loader.create_process("receiver", 0x1000);

    loader.create_channel("sender", "receiver", 42);

    assert_eq!(
        loader.get_channel_target("sender", 42),
        Some(p2),
        "Channel 42 should map sender → receiver"
    );

    assert_eq!(
        loader.get_channel_target("receiver", 42),
        None,
        "Channel mapping is unidirectional"
    );
}
