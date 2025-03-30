"""
    This is the main Python file which will start the Microkit process. Its main purpose
    is to scrape the provided .system XML file and dynamically link into the compiled
    C shared object binary (libmicrokit.so) to execute the necessary functions.

    TODO: Consider whether Python is the best language for this as it is quite slow.
    A few good contenders to maintain readability are Rust and Golang. Alternatively,
    this can also be done in C with an external library.

    Author: Michael Mospan (@mmospan)
"""

import ctypes
import xml.etree.ElementTree as ET
import signal

# --- The function that will be executed when the SIGSTP (Ctrl + Z) signal is received --- #
def handler(signum, frame):
    print("\nEXITING THE MICROKIT")
signal.signal(signal.SIGTSTP, handler)

# --- List of all the C functions which will be executed and their arg types --- #
libds = ctypes.CDLL('./libmicrokit.so')
libds.create_shared_memory.argtypes = [ctypes.c_char_p, ctypes.c_int]
libds.create_process.argtypes = [ctypes.c_char_p]
libds.add_shared_memory.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
libds.create_channel.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

# --- Grab the .system path and start a list of processes that will be scraped from it --- #
root = ET.parse("./example/example.system").getroot()
process_list = []

# --- Find all memory regions and call the C function `create_shared_memory` for each region --- #
for region in root.findall("memory_region"):
    region_name = ctypes.create_string_buffer(region.get("name").encode())
    region_size = int(region.get("size"), 16)
    libds.create_shared_memory(region_name, region_size)

# --- Find all protection domains and call the necessary C functions to create them --- #
for pd in root.findall("protection_domain"):
    # Create the necessary book keeping for each process
    pd_name = ctypes.create_string_buffer(pd.get("name").encode())
    libds.create_process(pd_name)

    # Add any shared memory it uses that was defined earlier
    pd_map = pd.find("map")
    pd_map_name = ctypes.create_string_buffer(pd_map.get("mr").encode())
    pd_map_varname = ctypes.create_string_buffer(pd_map.get("setvar_vaddr").encode())
    libds.add_shared_memory(pd_name, pd_map_name, pd_map_varname)

    # Add the process information to our process list. We will execute these all at once later.
    pd_image = pd.find("program_image")
    pd_image_path = "./build/" + pd_image.get("path")[:-3] + "so" # replace .elf with .so
    pd_image_path = ctypes.create_string_buffer(pd_image_path.encode())
    process_list.append((pd_name, pd_image_path))

# --- Find all communication channels between the processes and create them --- #
for channel in root.findall("channel"):
    ends = channel.findall("end")

    domain1 = ctypes.create_string_buffer(ends[0].get("pd").encode())
    id1 = int(ends[0].get("id"))
    domain2 = ctypes.create_string_buffer(ends[1].get("pd").encode())
    id2 = int(ends[1].get("id"))

    libds.create_channel(domain1, domain2, id1)
    libds.create_channel(domain2, domain1, id2)

# --- Finally, start all the processes within the process list --- #
# TODO: Since protection domains within the microkit can be ran in parallel,
# consider splitting this task up into multiple threads.
for tuple in process_list:
    libds.run_process(tuple[0], tuple[1])

# --- Wait for the microkit to exit, and clean up any used resources when it does --- #
try:
    libds.block_until_finish()
except:
    print("\nEXITING THE MICROKIT")