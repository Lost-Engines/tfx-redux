"""Small real-input helpers for the private X11 integration checks."""
import subprocess
import time


def move_game_pointer(environment,window,x,y):
 def run(*command):
  return subprocess.run(command,env=environment,check=True,capture_output=True,text=True,timeout=5)
 geometry=run('xdotool','getwindowgeometry','--shell',str(window)).stdout
 dimensions=dict(line.split('=',1)for line in geometry.splitlines()if '='in line)
 width,height=int(dimensions['WIDTH']),int(dimensions['HEIGHT'])
 available=min(width/320,height/200)
 scale=int(available) if available>=1 else available
 left=(width-int(320*scale))//2
 top=height-(height-int(200*scale))//2-int(200*scale)
 run('xdotool','mousemove','--window',str(window),str(round(left+x*scale)),str(round(top+y*scale)))


def click_game_pixel(environment,window,x,y):
 move_game_pointer(environment,window,x,y)
 def run(*command):
  return subprocess.run(command,env=environment,check=True,capture_output=True,text=True,timeout=5)
 run('xdotool','mousedown','1');time.sleep(.12);run('xdotool','mouseup','1')


def confirm_flight_quit(environment,window):
 """Confirm the native flight dialog; presentations/results can already exit."""
 deadline=time.monotonic()+5
 while time.monotonic()<deadline:
  result=subprocess.run(['xdotool','getwindowname',str(window)],env=environment,capture_output=True,text=True,timeout=5)
  if result.returncode:return # Presentation/result quit already closed its window.
  if any(name in result.stdout for name in ('TFX C++ — Quit','TFX Redux — Quit')):
   click_game_pixel(environment,window,132,108)
   return
  time.sleep(.05)
 raise RuntimeError('flight quit confirmation did not appear')
