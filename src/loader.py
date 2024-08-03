system = open("./user/example.system", "r")
output = open("./src/loader.txt", "w")

images = {}

line = system.readline()
while line:
    split = line.strip().split()
    if split and split[0] == "<memory_region":
        arg1 = split[1].split('\"')[1]
        arg2 = split[2].split('\"')[1]
        output.write(f"create_shared_memory {arg1} {arg2}\n")
    elif split and split[0] == "<protection_domain":
        process = split[1].split('\"')[1]
        output.write(f"create_process {process}\n")
    elif split and split[0] == "<program_image":
        image = "./user/" + split[1].split('\"')[1][:-3] + "so"
        images[process] = image
    elif split and split[0] == "<map":
        buffer = split[1].split('\"')[1]
        output.write(f"add_shared_memory {process} {buffer}\n")
    elif split and "<channel" in split[0]:
        split = system.readline().strip().split()
        from_domain = split[1].split('\"')[1]
        id1 = split[2].split('\"')[1]
        split = system.readline().strip().split()
        to_domain = split[1].split('\"')[1]
        id2 = split[2].split('\"')[1]
        output.write(f"create_channel {from_domain} {to_domain} {id1}\n")
        output.write(f"create_channel {to_domain} {from_domain} {id2}\n")
    line = system.readline()

for key in images:
    output.write(f"run_process {key} {images[key]}\n")