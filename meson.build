project('RSDKv5', ['c', 'cpp'],
    meson_version: '>= 0.59.0',
    default_options: ['warning_level=3', 'cpp_std=c++17'],
)

build_game = get_option('build_game')
static_game = get_option('static_game')
base_path = get_option('base_path')
platform = get_option('platform')
subsystem = get_option('subsystem')
rsdk_rev = get_option('rsdk_rev')
plus = get_option('plus')


sources = []
includes = []

defines = []
deps = []

# rsdk_name = 'RSDKv5'
# if rsdk_rev == 3
#     rsdk_name = 'RSDKv5U'
# endif

if static_game
    build_game = true
    defines += '-DRETRO_STANDALONE=0'
else
    defines += '-DRETRO_STANDALONE=1'
endif

if not plus
    defines += '-DRSDK_AUTOBUILD'
endif

defines += [
    f'-DRETRO_REVISION=@rsdk_rev@',
    f'-DBASE_PATH=@base_path@',
]

# Detect platform

if platform == 'auto'
    platform = target_machine.system()
endif

sub_auto = subsystem == 'auto'

if platform == 'windows'
    subsystem = sub_auto ? 'DX9' : subsystem
    if subsystem == 'auto'
    endif

    add_project_arguments(
        '-mwindows',
        '-Wl,-Bstatic,--whole-archive',
        '-Wl,--no-whole-archive',
        language: ['c', 'cpp'],
    )

    add_project_link_arguments(
        '-lwinpthread',
        '-static',
        '-static-libgcc',
        '-static-libstdc++',
        language: ['c', 'cpp'],
    )

    deps += [
        dependency('ogg'),
        dependency('theora'),
    ]
    
    if subsystem == 'DX9'
        add_project_link_arguments(
            '-ld3d9',
            '-lxinput',
            '-lcomctl32',
            '-lgdi32',
            '-lWinmm',
            '-lole32',
            '-lxaudio2_8',
            '-ld3dcompiler',
            language: ['c', 'cpp'],
        )
    elif subsystem == 'GL3'
        deps += dependency('glfw3')
        deps += dependency('glew')
        add_project_link_arguments(
            '-lxaudio2_8',
            language: ['c', 'cpp'],
        )
    elif subsystem == 'SDL2'
        deps += dependency('sdl2')
    endif
elif platform == 'android'
elif platform == 'linux'
    if subsystem == 'auto'
        subsystem = 'GL3'
    endif

    deps += [
        dependency('sdl2'),
        dependency('theora'),
        dependency('theoradec'),
        dependency('zlib'),
    ]

    # gl3 uses the same deps plus glfw and glew  
    if subsystem == 'GL3'
        # VIDEO: GLFW
        # INPUTS: Keyboard and GLFW
        # AUDIO: SDL2
        
        deps += [
            dependency('glfw3'),
            dependency('glew'),
        ]
    elif subsystem == 'SDL2'
        # EVERYTHING: SDL2
        # nothing to do here really
    endif
endif

defines += '-DRSDK_USE_'+subsystem

add_global_arguments('-fsigned-char', language: 'c')
add_global_arguments('-fsigned-char', '-fpermissive', language: 'cpp')

subdir('RSDKv5')
subdir('dependencies')

executable('RSDK', sources,
    include_directories: includes,
    dependencies: deps,
    c_args: defines,
    cpp_args: defines,
)
