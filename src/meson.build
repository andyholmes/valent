# The base API is defined by libvalent and functionality is delegated to plugins
subdir('libvalent')
subdir('plugins')

if get_option('tests')
  subdir('tests')
endif


# Executable
valent_c_args = ['-Wno-missing-declarations']
valent_link_args = []

valent = executable('valent', 'main.c',
              install: true,
               c_args: valent_c_args + release_args,
            link_args: valent_link_args,
           link_whole: plugins_static,
         dependencies: libvalent_dep,
  include_directories: [include_directories('.')],
                  pie: true,
)

