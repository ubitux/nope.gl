import yaml  # XXX drop this dep
import pprint
import subprocess
import os.path as op


def _gen_definitions_pyx(specs):
    # Map C nodes identifiers (NGL_NODE_*)
    content = 'cdef extern from "nodegl.h":\n'
    nodes_decls = []
    constants = []
    for node in specs.keys():
        if not node.startswith('_'):
            name = f'NODE_{node.upper()}'
            constants.append(name)
            nodes_decls.append(f'cdef int NGL_{name}')
    nodes_decls.append(None)
    content += '\n'.join((f'    {d}') if d else '' for d in nodes_decls) + '\n'
    content += '\n'.join(f'{name} = NGL_{name}' for name in constants) + '\n'
    return content


def _get_specs():
    if subprocess.call(['pkg-config', '--exists', 'libnodegl']) != 0:
        raise Exception(f'libnodegl is required to build pynodegl')
    data_root_dir = subprocess.check_output([pkg_config_bin, '--variable=datarootdir', 'libnodegl']).strip().decode()
    specs_file = op.join(data_root_dir, 'nodegl', 'nodes.specs')
    with open(specs_file) as f:
        specs = yaml.safe_load(f)
    content = _gen_definitions_pyx(specs)
    with open('nodes_def.pyx', 'w') as output:
        output.write(content)
    specs_dump = pprint.pformat(specs, sort_dicts=False)
    return f'SPECS = {specs_dump}'


if __name__ == '__main__':
    print(_get_specs())
