CFLAGS = -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I include
RM = rm -rf
CC = arm-unknown-linux-gnueabi-g++

CFLAGS+=-I/home/rohit/nasa/cross/include
LDFLAGS+=-L/home/rohit/nasa/cross/lib

# v4l2wrapper
CFLAGS += -I v4l2wrapper/inc

.DEFAULT_GOAL := all

# raspberry tools using ilclient
ILCLIENTDIR=/home/rohit/nasa/cross/hello_pi/libs/ilclient

CFLAGS  +=-I /home/rohit/nasa/cross/include/interface/vcos/ -I /home/rohit/nasa/cross/include/interface/vcos/pthreads/ -I /home/rohit/nasa/cross/include/interface/vmcs_host/linux/ -I $(ILCLIENTDIR) 
CFLAGS  += -DOMX_SKIP64BIT
LDFLAGS +=-L $(ILCLIENTDIR) -lpthread -lopenmaxil -lbcm_host -lvcos -lvchiq_arm /home/rohit/nasa/cross/lib/liblog4cpp.a

v4l2compress_omx: /home/rohit/nasa/cross/lib/liblog4cpp.a src/encode_omx.cpp src/v4l2compress_omx.cpp  $(ILCLIENTDIR)/libilclient.a libv4l2wrapper.a 
	$(CC) -o $@ $^ -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi $(CFLAGS) $(LDFLAGS) 

$(ILCLIENTDIR)/libilclient.a:
	make -C $(ILCLIENTDIR)
	
ALL_PROGS+=v4l2compress_omx

all: $(ALL_PROGS)

libyuv.a:
	git submodule init libyuv
	git submodule update libyuv
	make -C libyuv -f linux.mk
	mv libyuv/libyuv.a .
	make -C libyuv -f linux.mk clean

libv4l2wrapper.a: 
	git submodule init v4l2wrapper
	git submodule update v4l2wrapper
	make -C v4l2wrapper
	mv v4l2wrapper/libv4l2wrapper.a .
	make -C v4l2wrapper clean

	
upgrade:
	git submodule foreach git pull origin master
	
install:
	install -m 0755 $(ALL_PROGS) /usr/local/bin

clean:
	-@$(RM) $(ALL_PROGS) .*o *.a
