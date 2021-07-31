import win32event
import os

event_name = 'test_evt'
hEvent = win32event.CreateEvent(None, 0, 0, event_name) 
print("Waiting for event '%s' (handle: %d)" % (event_name, hEvent))
print("\nYou can set this event by running: ")
print("\tSignalAnyEvent.exe /n %s" % event_name)
print("\tSignalAnyEvent.exe /pn python.exe /h %u" % hEvent)
print("\tSignalAnyEvent.exe /pid %d /h %u" % (os.getpid(), hEvent))

while(True):
  win32event.WaitForSingleObject(hEvent, win32event.INFINITE)
  print("Event '%s' signaled" % event_name)
