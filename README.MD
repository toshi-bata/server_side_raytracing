# Server side Raytracing
This demo runs raytracing rendring in server side using offscreen buffer of HOOPS Luminate. In the server side, when a 3D CAD file is uploded and converted to SC model using HOOPS Exchange, its mesh data is also given to Luminate process. Server side rendering result is represented as background image of client side HC view.

## Dependencies
### SDKs in use (version)
* HOOPS Communicator (2024.5.0)
* HOOPS Exchange (2024.5.0)
* HOOPS Luminate (2024.5.0)

### Third paty module
* libmicrohttpd (1.0.0)<br>
  https://ftp.gnu.org/gnu/libmicrohttpd/
* Node.js (16.2.0)
* npm (8.19.4)
* ps-tree

### Tested server platforms
* Windows 11

## Setup
### Demo folders
&emsp;+ http root in releae mode
&emsp;&emsp;+ demo-app (copy demo-app folder from Communicator_SDK\web_viewer)<br>
&emsp;&emsp;+ engine.esm.wasm.js (copy engine.esm.wasm.js file from Communicator_SDK\web_viewer)<br>
&emsp;&emsp;+ hoops-web-viewer.mjs (copy hoops-web-viewer.js file from Communicator_SDK\web_viewer)<br>
&emsp;&emsp;+ server_side_raytracing<br>
&emsp;&emsp;&emsp;+ ExLuServer<br>
&emsp;&emsp;&emsp;&emsp;+ _VS2019.bat (for Windows)<br>
&emsp;&emsp;&emsp;+ win64 (copy win64 folder from Communicator_SDK\authoring\converter\bin)<br>
&emsp;&emsp;&emsp;&emsp;+ ExLuServer.exe<br>
&emsp;&emsp;&emsp;&emsp;+ REDCore.dll (copy from Luminate_SDK\bin\Win64)<br>
&emsp;&emsp;&emsp;&emsp;+ Embree.dll (copy from Luminate_SDK\bin\Win64)<br>
&emsp;&emsp;&emsp;+ win64d (copy win64d folder from Communicator_SDK\authoring\converter\bin)<br>
&emsp;&emsp;&emsp;&emsp;+ REDCore.dll (copy from Luminate_SDK\bin\Win64)<br>
&emsp;&emsp;&emsp;&emsp;+ Embree.dll (copy from Luminate_SDK\bin\Win64)<br>
&emsp;&emsp;&emsp;+ process_server<br>
&emsp;&emsp;&emsp;&emsp;+ index.js<br>
&emsp;&emsp;&emsp;&emsp;+ package.json<br>
&emsp;&emsp;&emsp;+ main.html<br>
&emsp;&emsp;&emsp;+ js<br>
&emsp;&emsp;&emsp;&emsp;+ main.js<br>
&emsp;&emsp;&emsp;+ HttpServer.js<br>
&emsp;&emsp;&emsp;+ package.json<br>

### Build ExServer
ExLuServer is a server side console application which is using HOOPS Exchange, Luminate and HC libconverter. <br>
#### Windows
1. Unzip HOOPS Exchange SDK and locate a valid hoops_license.h in `includ/` 
2. Unzip HOOPS Communicator SDK and locate a valid hoops_license.h in `authoring/libconverter/include/`
3. Unzip HOOPS Luminate SDK and locate a valid hoops_license.h in `include/`
4. Unzip libconverter SDK
5. Edit ExLuServer¥_VS2019.bat accoording to your environment and open ExLuServer.sln using the bat file
6. Build the solution (ExLuServer.exe will be created in `server_side_raytracing/win64` folder)
     
## Start demo / debug
1. Launch HTTP server with tarminal<br>
    `cd path/to/server_side_exchange`<br>
    `npm install`<br>
    `npm start`<br>
2. Start ExLuServer (giving a port number in command line argument)<br>
    Windows: `ExLuServer 8888`<br>
3. Open the main.html with server's port number (using Chrome)<br>
    `http://localhost:8000/main.html?viewer=SCS&instance=_empty.scs&port=8888`
Since one ExLuServer only supports one client, you need to start the ExLuServer with a different port number for each client. 

## Start release
Your HTTP server is running 
1. Launch process server with terminal <br>
    `cd path/to/server_side_raytracing/process_server`<br>
    `npm install`<br>
    `npm install ps-tree`<br>
    `npm start`<br>
2. Open the main.html without server's port number (using Chrome)<br>
    `http://your_domain_name/server_side_raytracing/main.html?viewer=SCS&instance=_empty.scs`