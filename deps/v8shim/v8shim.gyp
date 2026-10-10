{
  'targets': [
    {
      # V8's embedder API on top of the JavaScript engine that hosts the
      # WebAssembly instance (browser-v8).
      'target_name': 'v8shim',
      'type': 'static_library',
      'include_dirs': [ 'include', '.' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'include' ],
      },
      'cflags_cc': [ '-std=gnu++20' ],
      'sources': [
        'src/fatal.cc',
        'src/handles.cc',
        'src/isolate.cc',
        'src/misc.cc',
        'src/unimplemented.cc',
      ],
    },
  ],
}
