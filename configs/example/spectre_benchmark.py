import m5
from m5.objects import *


#Spectre
spectre = Process() # Added by Gururaj
spectre.executable = 'spectre'
spectre.cmd = [spectre.executable]
