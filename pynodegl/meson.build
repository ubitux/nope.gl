project('pynodegl', 'cython')

py = import('python').find_installation()
dep_py = py.dependency()

nodegl = dependency('libnodegl')

gen_specs_py = find_program('gen_specs.py')
specs_filename = 'specs.py'

specs_file = custom_target(
  specs_filename,
  command: gen_specs_py,
  capture: true,
  output: specs_filename,
)

py.extension_module(
  'pynodegl',
  '_pynodegl.pyx',
  dependencies: [dep_py, nodegl],
)
