import socket
import time
import thread
import colorsys
from ctypes import *
from struct import *
from Tkinter import *


#global values
currentIp = ""
currentIf = 0
interfaceDict = dict()
interval = 60 #seconds
can_width = 800
can_height = 600
max_y = 100
cur_if = None
cur_stat = None
xo = 30
yo = can_height - 30
ymax = 0
ymin = sys.maxint

#events handling
# canvas click
def clickCanvas(event):
    print "clicked at", event.x, event.y
    render()

def hostSelect(e):
    print hostList.curselection()
    print hostList.get(hostList.curselection())
    print interfaceDict[hostList.get(hostList.curselection())]
    global cur_if
    cur_if = hostList.get(hostList.curselection())
    render()

def statSelect(e):
    print statList.curselection()
    print statList.get(statList.curselection())
    global cur_stat
    cur_stat = statList.get(statList.curselection())
    render()


#graphics
master = Tk()
master.title("flow stat")

lrPane = PanedWindow(master)
lrPane.pack(fill=BOTH, expand=1)

controlPane = PanedWindow(lrPane, orient=VERTICAL)
controlPane.pack(fill=BOTH, expand=1)
lrPane.add(controlPane)

controlPaneLabel = Label(lrPane, text=" [Flow Stat Collector] ")
controlPane.add(controlPaneLabel)

controlPaneLabel_st = Label(lrPane, text="choose stat")
controlPane.add(controlPaneLabel_st)

#FLOW   IPKTS  RBYTES   IERRS   OPKTS  OBYTES   OERRS
statList = Listbox()
statList.insert(END, "ipkts")
statList.insert(END, "rbytes")
statList.insert(END, "ierrs")
statList.insert(END, "opkts")
statList.insert(END, "obytes")
statList.insert(END, "oerrs")
statList.activate(0)
statList.bind("<Button-1>", statSelect)
controlPane.add(statList)

controlPaneLabel_it = Label(lrPane, text="choose interfaces")
controlPane.add(controlPaneLabel_it)

def updateIfList():
    for hostif in interfaceDict:
        if not hostif in hostList.content:
	    hostList.content[hostif] = 1
	    hostList.insert(END, hostif)

hostList = Listbox()
hostList.content = dict()
updateIfList()

hostList.activate(0)
hostList.bind("<Button-1>", hostSelect)
controlPane.add(hostList)
canvas = Canvas(master, width=can_width, height= can_height)
canvas.bind("<Button-1>", clickCanvas)


lrPane.add(canvas)

colorPane = {0:"yellow",1:"purple",2:"black",3:"red",4:"green",5:"orange"}

#paint the canvas
def paintCanvas(canvas):
    print "painting the canvas"
    canvas.create_rectangle(1, 1, can_width, can_height, fill="gray")
    canvas.create_text((can_width/3,20),activefill="red",text="INTERFACE: "+str(cur_if), fill="black")
    canvas.create_text((can_width/3,40),activefill="red",text="STAT: "+str(cur_stat), fill="black")
    if cur_if == None or cur_stat == None : return
    global interval
    global colorPane
    # cal culate the max value
    global ymin
    global ymax
    ymin = sys.maxint
    ymax = 0
    ifcount = 0
    for fl in interfaceDict[cur_if]:
	ifcount+=1
        for stat in interfaceDict[cur_if][fl]:
	    if ymax < getattr(stat, cur_stat) : ymax = getattr(stat, cur_stat)
	    if ymin > getattr(stat, cur_stat) : ymin = getattr(stat, cur_stat)	
    print "range", ymax, ymin
    ymax += 200
    ymin -= 100
    # draw the x y
    global xo
    global yo
    offset = 20
    canvas.create_line(0, yo, can_width, yo)    
    canvas.create_line(0, can_height-yo, can_width,
        can_height-yo, fill="red", dash=(4, 4))
    canvas.create_line(xo, 0, xo, can_height, fill="red", dash=(4, 4))
    canvas.create_line(can_width-xo, 0, can_width-xo, can_height)
    canvas.create_text((can_width/2,yo +offset), activefill="black",
        text="time", fill="red")
    canvas.create_text((can_width-xo-offset,can_height/2), activefill="black",
        text="count", fill="blue")
    canvas.create_text((can_width-xo-offset,yo+offset), activefill="black",
        text="now", fill="red")
    canvas.create_text((xo+offset,yo+offset), activefill="black",
        text= str(interval)+"s before", fill="red")
    canvas.create_text((can_width-xo-offset,yo-offset), activefill="black",
        text=ymin, fill="blue")
    canvas.create_text((can_width-xo-offset, can_height-yo-offset),
	activefill="black", text=ymax, fill="blue")
    curif = 0
    for fl in interfaceDict[cur_if]:
        # show the color
	color = colorPane[curif]
        canvas.create_text((xo+offset,  offset * (1+ curif)),
	    activefill=color, text=fl, fill=color)
	curif+=1
        #draw dots
        global px, py
        px = -1
	py = -1
	for stat in interfaceDict[cur_if][fl]:
	    makeDot(interfaceDict[cur_if][fl], stat, color, canvas)

# previous dot
px = -1;
py = -1
def makeDot(lst, stat, color, canvas) :
    global interval
    global xo
    global yo
    offset = 6
    if (time.time() - stat.time) > interval :
	lst.remove(stat)
	return
    # get the x y
    val = getattr(stat, cur_stat) 
    y = yo - int(float(val-ymin)/float(ymax-ymin) * (can_height-xo*2))
    x = (can_width- xo)- int(float(time.time() - stat.time)
        /float(interval) * (can_width-xo*2))
    canvas.create_oval(x-offset/2,y-offset/2,x+offset/2,y+offset/2, fill =color)
    global py, px
    if px >0 and py >0:
        canvas.create_line(px, py, x, y, fill=color)
    px = x
    py = y
    canvas.create_text((x, y+10),text="-"
        +str(int(time.time() - stat.time)) + "/"+ str(val), fill=color)

#first paint
paintCanvas(canvas)

def render():
    print "re-rendering"
    global canvas
    paintCanvas(canvas)

# graphics refreshing thread
def refresh(dummy, interval):
    while True :
        render()
        time.sleep(interval)



# the receiver thread
def startReceiver(dummy, dummy1):
    UDP_IP = "10.132.146.129"
    UDP_PORT = 4096
    sock = socket.socket(socket.AF_INET, # Internet
                    socket.SOCK_DGRAM) # UDP
    sock.bind((UDP_IP, UDP_PORT))
    print "enter the receriving loop"
    while True:
        data = sock.recvfrom(1500) # buffer size is 1024 bytes
        read_datagram(data)


#analyze the sflow data
def read_datagram(data):
    new_data = SflowData(data)
    new_data.report()

    print "================="
    print "[", time.time(), "]"
    print "SFLOW DATAGRAM"
    print "| sflow version: ", new_data.readInt4B()
    ipVer = (new_data.readInt4B() + 1) * 2;
    print "| ip version: ", ipVer
    print "| datagram ip address: ", new_data.readIPv(ipVer)
    print "| sub agent id: ", new_data.readInt4B()
    print "| datagram id: ", new_data.readInt4B()
    print "| uptime: ", new_data.readInt4B()
    numSp = new_data.readInt4B()
    print "| samples: ", numSp
    for x in range(0, numSp):
	print "| read sample #", x+1	
	read_sample(new_data)

def read_sample(new_data):
    ef = new_data.readInt4B()
    ent = ef >> 20
    fmt = ef & 0x000FFFFF
    print "|| ent: ", ent, ",fmt: ", fmt
    print "|| len: ", new_data.readInt4B()
    if ent == 0 and fmt ==2 :
	read_counter_sample(new_data)
    else :
	print "format not supported! DROP sample!"
	return
 
def read_counter_sample(new_data):
    print "|| sample counter: ", new_data.readInt4B()
    iffi = new_data.readInt4B()
    iff= iffi >> 24
    ifi = iffi & 0x00FFFFFF
    print "|| if type: ", iff, ",if index: ", ifi
    global currentIf
    currentIf = ifi
    numcr = new_data.readInt4B()
    print "|| records: ", numcr
    for x in range(0, numcr):
	print "|| read record #", x+1	
	read_record(new_data)	

def read_record(new_data):
    ef = new_data.readInt4B()
    ent = ef >> 20
    fmt = ef & 0x000FFFFF
    print "||| ent: ", ent, ",fmt: ", fmt
    print "||| len: ", new_data.readInt4B()
    if ent == 52 and fmt ==52 :
	read_flow_stat(new_data)
    else :
	print "format not supported! DROP sample!"
	return

#FLOW   IPKTS  RBYTES   IERRS   OPKTS  OBYTES   OERRS
def read_flow_stat(new_data):
    flow_name =  new_data.readStr(32)
    print "||| name of flow: ", flow_name
    new_flow_stat = FlowStat(flow_name)
    temp = new_data.readInt4B()
    new_flow_stat.ipkts = temp
    print "||| IPKTS: ", temp
    temp = new_data.readInt4B()
    new_flow_stat.rbytes = temp
    print "||| RBYTES: ", temp
    temp = new_data.readInt4B()
    new_flow_stat.ierrs = temp
    print "||| IERRS: ", temp
    temp = new_data.readInt4B()
    new_flow_stat.opkts = temp
    print "||| OPKTS: ", temp
    temp = new_data.readInt4B()
    new_flow_stat.obytes = temp
    print "||| OBYTES: ", temp
    temp = new_data.readInt4B()
    new_flow_stat.oerrs = temp
    print "||| OERRS: ", temp
    print "new node:", (new_flow_stat)
    print "main dict key: ", (currentIp, currentIf)
    if not str((currentIp, currentIf)) in interfaceDict:
	interfaceDict[str((currentIp, currentIf))] = dict()
    flowDict = interfaceDict[str((currentIp, currentIf))]
    if not new_flow_stat.name in flowDict:
	flowDict[new_flow_stat.name] = list()
    flowDict[new_flow_stat.name].append(new_flow_stat)
    #print interfaceDict
    updateIfList()
    #render()
	
	    

# stat for a flow in on snapshot
class FlowStat:
    def __init__(self, name):
	self.name = str(name)
        self.time = time.time()
    def __str__(self):
	return ", ".join([self.name, str(self.time),
	str(self.ipkts), str(self.rbytes)])


# this is a class of raw packet    
class SflowData:
    pos = 0
    def __init__(self, data):
        self.data, self.address = data
    def set_pos(self, loc):
        self.pos = loc
    def mov_pos(self, dis):
        self.pos += dis
    def report(self):
        print "address is :", self.address
        print "pos is :", self.pos
        print "data is :", repr(self.data)
    def readInt4B(self):
      	#print "read>, ", repr(self.data[0:4])
        ret = (unpack('<I', self.data[self.pos: self.pos+4]))[0]
	self.pos += 4
	ret = socket.ntohl(ret)
	return ret
    def readIPv(self, ver):
	global currentIp
	if (ver == 4):
	    addr = socket.inet_ntoa((self.data[self.pos: self.pos+4]))
	    self.pos += 4
	    currentIp = addr
	    return addr
	addr = socket.inet_ntoa((self.data[self.pos: self.pos+16]))
	self.pos += 16
	currentIp = addr
	return addr
    def readStr(self, slen):
	s = self.data[self.pos: self.pos+slen]
	self.pos += slen
	return s


#"main()"
try:
    thread.start_new_thread( refresh, (2, 5) )
    thread.start_new_thread( startReceiver,(2, 2)  )
except:
    print "Unexpected error:", sys.exc_info()[0]
# TK has to take the main thread
master.mainloop()



