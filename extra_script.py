Import("env")

def skip_u8g2_fonts(env, node):
    return [
        n for n in node
        if "u8g2_fonts.c" not in n.get_path()
    ]

env.AddBuildMiddleware(skip_u8g2_fonts)