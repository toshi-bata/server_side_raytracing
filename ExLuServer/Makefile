ifeq ($(shell uname),Linux)
	BIN = /var/www/html/server_side_raytracing/linux64/ExLuServer
else
	BIN = /Users/toshi/SDK/Communicator/HOOPS_Communicator_2023_U1/authoring/converter/bin/macos/ExServer
endif

OBJS = main.cpp utilities.cpp ExProcess.cpp HLuminateServer.cpp ./hoops_luminate_bridge/src/AxisTriad.cpp ./hoops_luminate_bridge/src/ConversionTools.cpp ./hoops_luminate_bridge/src/HoopsExLuminateBridge.cpp ./hoops_luminate_bridge/src/HoopsLuminateBridge.cpp ./hoops_luminate_bridge/src/LightingEnvironment.cpp
CC = g++ -std=c++11

ifeq ($(shell uname),Linux)
	PATH_TO_INCLUDES = -I /usr/local/include \
					-I /var/hoops/HOOPS_Exchange_Publish_2024.8.0/include \
					-I /var/www/authoring/libconverter/include \
					-I /var/hoops/HOOPS_Luminate_2024.7.0/include

	PATH_TO_LIBCONVERTER = /var/www/authoring/converter/bin/linux64
else
	PATH_TO_INCLUDES = -I /usr/local/include \
			-I /Users/toshi/SDK/Exchange/HOOPS_Exchange_2023_U1/include \
			-I /Users/toshi/SDK/Communicator/HOOPS_Communicator_2023_U1/authoring/libconverter/include

	PATH_TO_LIBCONVERTER = /Users/toshi/SDK/Communicator/HOOPS_Communicator_2023_U1/authoring/converter/bin/macos
endif

PATH_TO_LIBMHD_LIBS = /usr/local/lib

LDLIBS = -ldl -lstdc++ \
	 -L$(PATH_TO_LIBMHD_LIBS) \
	 -lmicrohttpd \
	 -L$(PATH_TO_LIBCONVERTER) \
	 -lconverter \
	 -lstdc++fs

$(BIN): $(OBJS)
	$(CC) $(PATH_TO_INCLUDES) $(OBJS) -o $(BIN) $(LDLIBS)

.PHONY: clean
clean:
	rm -f $(BIN)