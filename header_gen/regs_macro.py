import sys
import json

with open("config.json", "r") as f:
    base_data = json.load(f)
    if "ofst_mask" not in base_data:
        base_data["ofst_mask"] = []
    data = base_data["ofst_mask"]
    struct_name = base_data["struct_name"]
    var_name = base_data["var_name"]
    gen_type = base_data["gen_type"]

def add_reg(*args):
    base, reg, field, shift, mask_size = args
    data.append([base, reg, field, int(shift), int(mask_size)])

def get_base_name(*args):
    base, reg, field = args
    base, reg, field = base.upper(), reg.upper(), field.upper()
    return f"{base}_{reg}_{field}"

def format_setter(base, reg, field):
    name = get_base_name(base, reg, field)
    fn_name = "set_" + name.lower()
    mask = name + "_MASK"
    access = f"{var_name}->{reg}"
    var_decl = f"struct {struct_name} *{var_name}"

    if gen_type == "user":
        return f"static inline void {fn_name}(volatile {var_decl}) " "{\n" \
            f"    {access} |= {mask};\n" \
            "}\n"
    elif gen_type == "linux":
        return f"static inline void {fn_name}(__iomem {var_decl}) " "{\n" \
            f"\tint new_value = ioread8(&{access}) | {mask};\n" \
            f"\tiowrite8(new_value, &{access});\n" \
            "}\n"

def format_unsetter(base, reg, field):
    name = get_base_name(base, reg, field)
    fn_name = "unset_" + name.lower()
    mask = name + "_MASK"
    access = f"{var_name}->{reg}"
    var_decl = f"struct {struct_name} *{var_name}"

    if gen_type == "user":
        return f"static inline void {fn_name}(volatile {var_decl}) " "{\n" \
            f"    {access} &= ~{mask};\n" \
            "}\n"
    elif gen_type == "linux":
        return f"static inline void {fn_name}(__iomem {var_decl}) " "{\n" \
            f"\tint new_value = ioread8(&{access}) & ~{mask};\n" \
            f"\tiowrite8(new_value, &{access});\n" \
            "}\n"

def format_getter(base, reg, field):
    name = get_base_name(base, reg, field)
    fn_name = "get_" + name.lower()
    mask = name + "_MASK"
    ofst = name + "_OFST"
    access = f"{var_name}->{reg}"
    var_decl = f"struct {struct_name} *{var_name}"

    if gen_type == "user":
        return f"static inline int {fn_name}(volatile {var_decl}) " "{\n" \
            f"    return ({access} & {mask}) >> {ofst};\n" \
            "}\n"
    elif gen_type == "linux":
        return f"static inline int {fn_name}(__iomem {var_decl}) " "{\n" \
            f"\treturn (ioread8(&{access}) & {mask}) >> {ofst};\n" \
            "}\n"

def gen_regs():
    for base, reg, field, shift, mask_size in data:
        name = get_base_name(base, reg, field)
        mask = "1" * mask_size
        print(f"#define {name}_OFST ({shift})")
        print(f"#define {name}_MASK ({mask} << {name}_OFST)")
        print()

def gen_funcs(rg=None, *fields):
    for base, reg, field, *args in data:
        if rg is not None and (reg != rg or field not in fields):
            continue
        print(format_getter(base, reg, field))
        print(format_setter(base, reg, field))
        print(format_unsetter(base, reg, field))

def gen_all(scratch_file=None):
    if scratch_file is not None:
        with open(scratch_file, "r") as f:
            print(f.read())
    gen_regs()
    gen_funcs()

vtb = {
    "gen_regs": gen_regs,
    "gen_funcs": gen_funcs,
    "gen": gen_all
}

vtb[sys.argv[1]](*sys.argv[2:])
