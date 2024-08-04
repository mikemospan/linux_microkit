import ctypes
import xml.etree.ElementTree as ET

root = ET.parse("./user/example.system").getroot()

libds = ctypes.CDLL('./libmain.so')
libds.create_shared_memory.argtypes = [ctypes.c_char_p, ctypes.c_int]
libds.create_process.argtypes = [ctypes.c_char_p]
libds.add_shared_memory.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
libds.create_channel.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

process_and_elf = []

for region in root.findall("memory_region"):
    region_name = ctypes.create_string_buffer(region.get("name").encode())
    region_size = int(region.get("size"), 16)
    libds.create_shared_memory(region_name, region_size)

for domain in root.findall("protection_domain"):
    domain_name = ctypes.create_string_buffer(domain.get("name").encode())
    libds.create_process(domain_name)

    domain_map = domain.find("map")
    domain_map_name = ctypes.create_string_buffer(domain_map.get("mr").encode())
    libds.add_shared_memory(domain_name, domain_map_name)

    domain_image = domain.find("program_image")
    domain_image_path = "./user/" + domain_image.get("path")[:-3] + "so"
    domain_image_path = ctypes.create_string_buffer(domain_image_path.encode())
    process_and_elf.append((domain_name, domain_image_path))

for channel in root.findall("channel"):
    ends = channel.findall("end")

    domain1 = ctypes.create_string_buffer(ends[0].get("pd").encode())
    id1 = int(ends[0].get("id"))
    domain2 = ctypes.create_string_buffer(ends[1].get("pd").encode())
    id2 = int(ends[1].get("id"))

    libds.create_channel(domain1, domain2, id1)
    libds.create_channel(domain2, domain1, id2)

for tuple in process_and_elf:
    libds.run_process(tuple[0], tuple[1])

libds.block_until_finish()