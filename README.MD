# Server side Raytracing
This demo runs raytracing rendring in server side using offscreen buffer of HOOPS Luminate. In the server side, when a 3D CAD file is uploded and converted to SC model using HOOPS Exchange, its mesh data is also given to Luminate process. Server side rendering result is represented as background image of client side HC view.

## Dependencies
### SDKs in use (version)
* HOOPS Communicator (2024.1.0)
* HOOPS Exchange (2024.1.0)
* HOOPS Luminate (2024.1.0)

### Third paty module
* libmicrohttpd (1.0.0)<br>
  https://ftp.gnu.org/gnu/libmicrohttpd/

### Tested server platforms
* Windows 11

## Setup
### Demo folders
&emsp;+ server_side_raytracing<br>
&emsp;&emsp;+ ExLuServer<br>
&emsp;&emsp;&emsp;+ _VS2019.bat (for Windows)<br>
&emsp;&emsp;+ main.html<br>
&emsp;&emsp;+ js<br>
&emsp;&emsp;&emsp;+ main.js<br>
&emsp;&emsp;+ HttpServer.js<br>
&emsp;&emsp;+ package.json<br>
&emsp;+ src (copy src folder here from <Communicator SDK>\web_viewer)<br>

### Build ExServer
ExLuServer is a server side console application which is using HOOPS Exchange, Luminate and HC libconverter. <br>
#### Windows
1. Unzip HOOPS Exchange SDK and locate a valid hoops_license.h in `includ/` 
2. Unzip HOOPS Communicator SDK and locate a valid hoops_license.h in `authoring/libconverter/include/`
3. Unzip libconverter SDK
4. Edit ExLuServer¥_VS2019.bat accoording to your environment and open ExLuServer.sln using the bat file
5. Build the solution
     
## Start demo
1. Launch HTTP server with tarminal<br>
    `cd path/to/server_side_exchange`<br>
    `npm install`<br>
    `npm start`<br>
2. Start Communicator SC server (<Communicator SDK>/quick_start/start_server)
3. Register the following enviroment valiables<br>
    Name: `EX_SERVER_WORKING_DIR`&emsp;Value: `path¥to¥temp¥folder`<br>
    Name: `SC_MODELS_DIR`&emsp;Value: `path¥to¥sc_models¥folder`<br>
    Name: `HTTP_ROOT_DIR`&emsp;Value: `path¥to¥server_side_raytracing¥folder`<br>
4. Start ExLuServer (giving a port number in command line argument)<br>
    Windows: `ExServer 8888`<br>
5. Open the main.html with server's port number (using Chrome)<br>
    `http://localhost:8000/main.html?viewer=CSR&instance=_empty&port=8888`
    

