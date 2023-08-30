import os
import sys
from pynopegl_utils.viewer import run

os.environ['PATH'] = ":".join([sys._MEIPASS + '/bin/', os.environ['PATH']])
print(os.environ['PATH'])

run()
