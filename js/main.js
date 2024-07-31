import * as Communicator from "@hoops/web-viewer";
import { createViewer } from "/js/create_viewer.js";
import { SetMaterialOperator } from "/js/SetMaterialOp.js";
import { CreateFloorOperator } from "/js/CreateFloorOp.js";
class Main {
    constructor() {
        this._viewer;
        this._port;
        this._viewerMode;
        this._reverseProxy;
        this._sessionId;
        this._serverCaller;
        this._tol = 1e-4;
        this._units = {
            "1.0": "mm",
            "25.4": "inch",
            "10.0": "cm",
            "1000.0": "m"
        };
        this._timerId;
        this._isBusy;
        this._prevCamera;
        this._setMaterialOp;
        this._setMaterialOpHandle;
        this._createFloorOp;
        this._createFloorOpHandle;
        this._materialDB;
        this._lightingDB;
        this._currentMaterialName;
        this._currentMaterialId;
        this._currentLightingId;
        this._isAutoSlide;
        this._floorMeshId;
        this._floorColor = [];
        this._rotationCenter;
    }

    start (port, viewerMode, modelName, reverseProxy) {
        this._port = port;
        this._viewerMode = viewerMode.toUpperCase();
        this._reverseProxy = reverseProxy;
        this._timerId = null;
        this._isBusy = false;
        this._floorMeshId = null;
        this._requestServerProcess();
        this._createViewer(this._viewerMode, modelName);
        this._initEvents();
        this._invokeNew();
    }

    _requestServerProcess () {
        this._sessionId = create_UUID();

        // Create PsServer caller
        let psServerURL = window.location.protocol + "//" + window.location.hostname + ":" + this._port;
        if (this._reverseProxy) {
            psServerURL = window.location.protocol + "//" + window.location.hostname + "/httpproxy/" + this._port;
        }

        this._serverCaller = new ServerCaller(psServerURL, this._sessionId);
    }

    _createViewer(viewerMode, modelName) {
        createViewer(viewerMode, modelName, "container", this._reverseProxy).then((hwv) => {
            this._viewer = hwv;

            this._viewer.setCallbacks({
                sceneReady: () => {
                    this._viewer.view.setBackgroundColor(null, null);
                    this._viewer.view.axisTriad.enable(true);
                    this._viewer.view.axisTriad.setAnchor(Communicator.OverlayAnchor.LowerRightCorner);
                },
                camera: (camera) => {
                    const root = this._viewer.model.getAbsoluteRootNode();
                    this._viewer.model.resetNodesOpacity([root]);
                    if (this._isAutoSlide) {
                        $('#backgroundImg').attr('src', 'css/images/default_background.png');
                    }
                    $('#progress').hide();

                    let isOn = $('[data-command="Raytracing"]').data("on");
                    if (isOn) {
                        this._clearRaytracing();
                        this._prevCamera = camera;

                        setTimeout(() => {
                            if (camera.equals(this._prevCamera)) {
                                const params = this._getRenderingParams();
                                this._serverCaller.CallServerPost("SyncCamera", params).then(() => {
                                    this._invokeDraw();
                                });
                            }
                          }, "1000");
                    }
                },
                timeoutWarning: () => {
                    // If rendering process is going on, continue the process
                    if (null != this._timerId) {
                        this._viewer.setClientTimeout(15, 14);
                    }
                },
                timeout: () => {
                    //this._serverCaller.CallServerPost("Clear");
                    console.log("Timeout");
                }
            });

            this._setMaterialOp = new SetMaterialOperator(this._viewer, this);
            this._setMaterialOpHandle = this._viewer.registerCustomOperator(this._setMaterialOp);
            this._createFloorOp = new CreateFloorOperator(this._viewer, this);
            this._createFloorOpHandle = this._viewer.registerCustomOperator(this._createFloorOp);

            this._viewer.start();
        });
    }

    _initEvents() {
        let resizeTimer;
        let interval = Math.floor(1000 / 60 * 10);
        $(window).resize((e) => {
            if (resizeTimer !== false) {
                clearTimeout(resizeTimer);
            }
            resizeTimer = setTimeout(() => {
                this._viewer.resizeCanvas();

                // Resize Lumionate
                let isOn = $('[data-command="Raytracing"]').data("on");
                if (isOn) {
                    const params = this._getRenderingParams();
                    this._serverCaller.CallServerPost("Resize", params).then(() => {
                    });
                }

            }, interval);
        });

        // Before page reload or close
        $(window).on('beforeunload', (e) => {
            this._serverCaller.CallServerPost("Clear");
        });

        // File uploading
        $("#Upload3DDlg").dialog({
            autoOpen: false,
            height: 380,
            width: 450,
            modal: true,
            title: "CAD file upload",
            closeOnEscape: true,
            position: {my: "center", at: "center", of: window},
            buttons: {
                'Cancel': () => {
                    $("#Upload3DDlg").dialog('close');
                }
            }
        });

        $('#cadFileSelect').change((e) =>{
            if (0 < e.target.files.length)
                $('#cadFileSubmit').click();
        });

        $(".uploadDlg").submit((e) => {
            // Cancel default behavior (abort form action)
            e.preventDefault();

            if (0 == e.target[0].files.length) {
                alert("Please select a file.");
                return;
            }

            const formData = new FormData(e.target);

            // Get file name
            let fileName = e.target[0].files[0].name;
            let fileType = fileName.split('.').pop();

            if ("hdr" == fileType) {
                this._loadEnv(formData);
                $('#UploadEnvDlg').dialog('close');
            }
            else {
                const scModelName = "model";

                // Get options
                const params = {
                }

                this._loadModel(params, formData, scModelName);
                $('#Upload3DDlg').dialog('close');
            }
        });

        // Prepare Material dialog
        $("#materialsDlg").dialog({
            autoOpen: false,
            height: 550,
            width: 350,
            modal: false,
            title: "Materials",
            closeOnEscape: true,
            position: {my: "left top", at: "left bottom", of: "#toolbarGr"},
            buttons: {
                'Close': () => {
                    $("#materialsDlg").dialog('close');
                }
            },
            open: () => {
                // Close Add Floor command
                if ($('#addFloorDlg').dialog('isOpen')) $("#addFloorDlg").dialog('close');

                this._viewer.operatorManager.push(this._setMaterialOpHandle);
                $('[data-command="SetMaterial"]').data("on", true).css("background-color", "khaki");
            },
            close: () => {
                this._setDefaultOperators();
                $('[data-command="SetMaterial"]').data("on", false).css("background-color", "gainsboro");
            }
        });
        $('#checkOverrideMaterial').prop('checked', true);

        // Load material list from JSON file
        const url = "MaterialLibrary/Catalog/material_list.json";
        $.getJSON(url, (obj) => {
            this._materialDB = obj;

            // Initialize Material type combobox 
            for (let material in this._materialDB) {
                $('#matTypeId').append($('<option>').html(material).val(material));
            }

            // Set default material type
            $('#matTypeId').val("Metal");
            this._setMaterialThumbnail("Metal");
        });

        // Material type chang event handler
        $('#matTypeId').change((e) => {
            const val = $(e.currentTarget).val();
            this._setMaterialThumbnail(val);
        });

        // Prepare Lighting Mode dialog
        $("#lightingModeDlg").dialog({
            autoOpen: false,
            height: 420,
            width: 350,
            modal: false,
            title: "Lighting Mode",
            closeOnEscape: true,
            position: {my: "left top", at: "left bottom", of: "#toolbarGr"},
            buttons: {
                'Close': () => {
                    $("#lightingModeDlg").dialog('close');
                }
            }
        });

        // Env file uploading
        $("#UploadEnvDlg").dialog({
            autoOpen: false,
            height: 300,
            width: 450,
            modal: true,
            title: "Env file upload",
            closeOnEscape: true,
            position: {my: "center", at: "center", of: window},
            buttons: {
                'Cancel': () => {
                    $("#UploadEnvDlg").dialog('close');
                }
            }
        });
        $('#envFileSelect').change((e) =>{
            if (0 < e.target.files.length)
                $('#envFileSubmit').click();
        });
        
        // Up Vector dialog
        $("#upVectorDlg").dialog({
            autoOpen: false,
            height: 250,
            width: 350,
            modal: false,
            title: "Up Vector",
            closeOnEscape: true,
            position: {my: "left top", at: "left bottom", of: "#toolbarGr"},
            buttons: {
                'Close': () => {
                    $("#upVectorDlg").dialog('close');
                }
            },
        });

        $('#upVector').change((e) => {
            this._changeUpVector();
        });

        $("#rotationAng").change((e) => {
            this._changeUpVector();
        });

        // Add floor dialog
        $("#addFloorDlg").dialog({
            autoOpen: false,
            height: 460,
            width: 450,
            modal: false,
            title: "Add Floor",
            closeOnEscape: true,
            position: {my: "left top", at: "left bottom", of: "#toolbarGr"},
            buttons: {
                'Close': () => {
                    $("#addFloorDlg").dialog('close');
                }
            },
            open: () => {
                // Close Set Material command
                if ($('#materialsDlg').dialog('isOpen')) $("#materialsDlg").dialog('close');

                // Hide progress bar while the command
                if ($('[data-command="Raytracing"]').data("on")) $('#progress').hide();

                this._viewer.operatorManager.push(this._createFloorOpHandle);
                this._createFloorOp.Open();
                $('[data-command="CreateFloor"]').data("on", true).css("background-color", "khaki");
            },
            close: () => {
                // Show progress bar
                if ($('[data-command="Raytracing"]').data("on")) $('#progress').show();

                this._createFloorOp.Close();
                this._setDefaultOperators();
                $('[data-command="CreateFloor"]').data("on", false).css("background-color", "gainsboro");
            }
        });
        // Create color picker
        $('.simple_color').simpleColor({
            cellWidth: 9,
            cellHeight: 9,
            onSelect: (hex, element) => {
                this._floorColor = hex2rgb(hex);
            }
        });

        $("#addFloorDlg").submit((e) => {
            // Cancel default behavior (abort form action)
            e.preventDefault();

            let formData = null;
            if (0 != e.target[2].files.length) {
                formData = new FormData(e.target);
            }

            this._updateFloorMaterial(formData);
        });

        // Progress bar
        $("#progressBar").progressbar({
            value: 0,
            change: () => {
              $("#progressTxt").text($("#progressBar").progressbar("value").toFixed(1) + "%");
            },
            complete: function() {
              $("#progressTxt").text($("#progressBar").progressbar("value").toFixed(1) + "% completed");
            }
          });

        // Command
        $(".toolbarBtn").on("click", (e) => {
            let command = $(e.currentTarget).data("command");
            let isOn = $(e.currentTarget).data("on");

            switch(command) {
                case "New": {
                    this._invokeNew();
                } break;
                case "Upload": {
                    $('#Upload3DDlg').dialog('open');
                    $('#cadFileSelect').click();
                } break;
                case "Raytracing": {
                    if (!isOn) {
                        $(e.currentTarget).data("on", true).css("background-color", "khaki");
                        this._invokeRaytracing(command);
                    }
                    else {
                        $(e.currentTarget).data("on", false).css("background-color", "gainsboro");
                        this._clearRaytracing();
                    }
                } break;
                case "SetMaterial": {
                    if (!isOn) {
                        $('#materialsDlg').dialog('open');
                    }
                    else {
                        $('#materialsDlg').dialog('close');
                    }
                } break;
                case "SetLighting": {
                    $('#lightingModeDlg').dialog('open');
                } break;
                case "UpVector": {
                    $('#upVectorDlg').dialog('open');
                } break;
                case "CreateFloor": {
                    if (!isOn) {
                        $('#addFloorDlg').dialog('open');
                    }
                    else {
                        $('#addFloorDlg').dialog('close');
                    }
                } break;
                case "DeleteFloor": {
                    this._deleteFloor();
                } break;
                case "Download": {
                    const serverName = this._sessionId + "/image.png";
                    const downloadName = "image.png";
                    this._downloadImage(serverName, downloadName);

                } break;
                default: {} break;
            }
        });

        // Normal button common
        $(".normalBtn").mousedown((e) => {
            $(e.currentTarget).data("on", true).css("background-color", "khaki");
        }).mouseup((e) => {
            $(e.currentTarget).data("on", false).css("background-color", "gainsboro");
        });

        // Slider
        $("#opacitySlider").slider({
            value: 1,
            min: 0,
            max: 1,
            step: 0.01,
            slide: (e, ui) => {
                this._isAutoSlide = false;

                const opacity = ui.value;
                const root = this._viewer.model.getAbsoluteRootNode();
                this._viewer.model.setNodesOpacity([root], opacity);
            },
            change: (e, ui) => {
            },
        });
    }

    _setDefaultOperators() {
        this._viewer.operatorManager.clear();
        this._viewer.operatorManager.push(Communicator.OperatorId.Navigate);
        this._viewer.operatorManager.push(Communicator.OperatorId.Select);
        this._viewer.operatorManager.push(Communicator.OperatorId.Cutting);
        this._viewer.operatorManager.push(Communicator.OperatorId.Handle);
        this._viewer.operatorManager.push(Communicator.OperatorId.NavCube);
        this._viewer.operatorManager.push(Communicator.OperatorId.AxisTriad);
    }

    _invokeNew() {
        return new Promise((resolve, reject) => {
            // Stop rendering
            this._clearRaytracing();

            // Close Set Material dialog
            if ($('#materialsDlg').dialog('isOpen')) $("#materialsDlg").dialog('close');

            // Close Add Floor dialog
            if ($('#addFloorDlg').dialog('isOpen')) $("#addFloorDlg").dialog('close');

            // Set default values of Add Floor dialog
            $('.simple_color').setColor('#ffffff');
            this._floorColor = [255, 255, 255];
            $('#colorA').val(1.0);
            $('#textureScale').val(1.0);
            $('#textureFileSelect').val('');

            // Delete floor mesh
            this._deleteFloor();

            // Set default values of Up Vector dialog
            $("#upVector").val("Z");
            $("#rotationAng").val(0);

            // Set toolbar default
            $('[data-command="Raytracing"]').data("on", false);
            $('[data-command="Raytracing"]').prop("disabled", true).css("background-color", "darkgrey");
            $('.while_rendering').prop("disabled", true).css("background-color", "darkgrey");

            if (undefined != this._materialDB) {
                // Set default material type
                $('#matTypeId').val("Metal");
                this._setMaterialThumbnail("Metal");
            }

            // Load default lighting list from JSON file
            const lighting_url = "Lighting/lighting_list.json";
            $.getJSON(lighting_url, (obj) => {
                if (undefined != obj.default) {
                    this._lightingDB = obj.default;

                    // Lighting thumbnail click event handler
                    this._setLightingThumbClickHandler();

                    this._currentLightingId = 1;
                    $(`.itemList_thumbnail:nth-child(${this._currentLightingId})` + '.lightingItem').addClass('thumbnail-selected');

                }
            });

            if (undefined != this._viewer) {
                // Delete model
                let promiseArr = [];
                const root = this._viewer.model.getAbsoluteRootNode();
                const childNodes = this._viewer.model.getNodeChildren(root);
                for (let node of childNodes) {
                    promiseArr.push(this._viewer.model.deleteNode(node));
                }

                Promise.all(promiseArr).then(() => {
                    // Reset server
                    this._serverCaller.CallServerPost("Clear").then((res) => {
                        this._requestServerProcess();
                        // Set default opetators
                        this._setDefaultOperators();

                        return resolve(res);
                    });
                });
            }
            else {
                return resolve();
            }
        });
    }

    _setMaterialThumbnail(matType) {
        $('#materialList').empty();

        const materials = this._materialDB[matType];
        // Set thumbnails
        for (let mat of materials) {
            const image = new Image();
            image.src = "MaterialLibrary/Catalog/" + mat.img;

            let $ele = $('<div />', {class:'itemList_thumbnail materialItem',title: mat.name,'data-file': mat.redFile});
            $($ele).append(image);
            $($ele).append(mat.name);

            $('#materialList').append($ele);

        }

        // Set event handler
        $('.itemList_thumbnail').on('click', (e) => {
            const cls = $(e.currentTarget).attr('class');
            if (cls.match('materialItem')) {
                this._currentMaterialName = e.currentTarget.dataset.file;

                // Highlight selected thumbnail
                const id = $('.itemList_thumbnail' + '.materialItem').index(e.currentTarget) + 1

                if (this._currentMaterialId == id)
                    return;

                $(`.itemList_thumbnail:nth-child(${this._currentMaterialId})` + '.materialItem').removeClass('thumbnail-selected');
                this._currentMaterialId = id;
                $(`.itemList_thumbnail:nth-child(${this._currentMaterialId})` + '.materialItem').addClass('thumbnail-selected')
            }
        });

        // Set default
        // Material list
        this._currentMaterialId = 1;
        $(`.itemList_thumbnail:nth-child(${this._currentMaterialId})` + '.materialItem').addClass('thumbnail-selected')
        this._currentMaterialName = materials[0].redFile;

        // Lighting mode list
        this._currentLightingId = 1;
        $(`.itemList_thumbnail:nth-child(${this._currentMaterialId})` + '.lightingItem').addClass('thumbnail-selected')
    }

    async _loadModel(params, formData, scModelName) {
        $("#loadingImage").show();
        const now = new Date().getTime();
        $('#backgroundImg').attr('src', 'css/images/default_background.png');

        const res = await this._invokeNew();
        if ("success" != res) {
            $("#loadingImage").hide();
            alert("Server is busy.");
            return;
        }

        await this._serverCaller.CallServerPost("SetOptions", params);
        const arr = await this._serverCaller.CallServerSubmitFile(formData);
        if (0 == arr[0]) return;

        const root = this._viewer.model.getAbsoluteRootNode();
        const childNodes = this._viewer.model.getNodeChildren(root);
        for (let node of childNodes) {
            await this._viewer.model.deleteNode(node);
        }
        const config = new Communicator.LoadSubtreeConfig();
        if (this._viewerMode == "CSR" || this._viewerMode == "SSR") {
            await this._viewer.model.loadSubtreeFromModel(root, this._sessionId + "/" + scModelName, config);
        }
        else {
            await this._viewer.model.loadSubtreeFromScsFile(root, this._sessionId  + "/" + scModelName + ".scs", config);
        }
        const camera = this._viewer.view.getCamera();
        camera.setProjection(Communicator.Projection.Perspective);
        this._viewer.view.setCamera(camera);

        await this._serverCaller.CallServerPost("PrepareRendering", this._getRenderingParams());
        $('[data-command="Raytracing"]').prop("disabled", false).css("background-color", "gainsboro");
        $("#loadingImage").hide();
    }

    _loadEnv(formData) {
        $("#loadingImage").show();

        this._serverCaller.CallServerSubmitFile(formData).then((arr) => {
            $("#loadingImage").hide();
            if (0 < arr.length) {
                if (1 == arr[0]) {
                    // Add env map thumbnail 
                    const id = this._lightingDB.length - 1;
                    this._lightingDB.push({
                        id: this._lightingDB.length,
                        name: 'My EnvMap-' + String(id), 
                        img: this._sessionId + "/EnvMapThumb_" + String(id) + ".png"
                    });

                    this._setLightingThumbClickHandler();

                    this._currentLightingId = this._lightingDB.length;
                    $(`.itemList_thumbnail:nth-child(${this._currentLightingId})` + '.lightingItem').addClass('thumbnail-selected');
                }
            }
        });
    }

    _setLightingThumbClickHandler() {
        // Set default lightings
        $('#lightingList').empty();

        const list = JSON.parse(JSON.stringify(this._lightingDB));

        // Add load env map item
        list.push({
            id: this._lightingDB.length,
            name: "Load Environment Map", 
            img: "Load.png"
        });

        // Set thumbnails
        for (let item of list) {
            const image = new Image();
            if (-1 == item.img.indexOf("/EnvMapThumb_")) {
                image.src = "Lighting/" + item.img;
            }
            else {
                image.src = item.img;
            }

            let $ele = $('<div />', {class:'itemList_thumbnail lightingItem',title: item.name});
            $($ele).append(image);
            $($ele).append(item.name);

            $('#lightingList').append($ele);
        }
        
        // Lighting thumbnail click event handler
        $('.itemList_thumbnail').on('click', (e) => {
            const cls = $(e.currentTarget).attr('class');
            if (cls.match('lightingItem')) {
                // Highlight selected thumbnail
                const id = $('.itemList_thumbnail' + '.lightingItem').index(e.currentTarget) + 1

                if (this._currentLightingId == id)
                    return;

                $(`.itemList_thumbnail:nth-child(${this._currentLightingId})` + '.lightingItem').removeClass('thumbnail-selected');

                if (this._lightingDB.length + 1 == id) {
                    $('#UploadEnvDlg').dialog('open');
                    $('#envFileSelect').click();
                }
                else {
                    this._currentLightingId = id;
                    $(`.itemList_thumbnail:nth-child(${this._currentLightingId})` + '.lightingItem').addClass('thumbnail-selected');

                    const params = {
                        lightingId: this._currentLightingId - 1
                    }
            
                    this._serverCaller.CallServerPost("SetLighting", params);
                }
            }
        });
    }

    _getRenderingParams() {
        const size = this._viewer.view.getCanvasSize();
        const width = size.x;
        const height = size.y;

        const camera = this._viewer.view.getCamera();
        const target = camera.getTarget();
        const up = camera.getUp();
        const position = camera.getPosition();
        const projection = camera.getProjection();
        const cameraW = camera.getWidth();
        const cameraH = camera.getHeight();
        const params = {
            width: width,
            height: height,
            target: [target.x, target.y, target.z],
            up: [up.x, up.y, up.z],
            position: [position.x, position.y, position.z],
            projection: projection,
            cameraW: cameraW,
            cameraH: cameraH
        };
        return params;
    }

    _invokeRaytracing(command) {   
        $("#loadingImage").show();

        const params = this._getRenderingParams();

        this._serverCaller.CallServerPost(command, params).then(() => {
            // Enabling commands
            $('.while_rendering').prop("disabled", false).css("background-color", "gainsboro");

            $("#loadingImage").hide();
            this._invokeDraw();
        });
    }

    _invokeDraw() {
        $("#progressBar").progressbar("value", 0);
        $('#progress').show();
        this._isAutoSlide = true;

        this._timerId = setInterval(() => {
            if (false == this._isBusy && 0 < this._timerId) {
                this._isBusy = true;
                this._serverCaller.CallServerPost("Draw", "{}", "FLOAT").then((arr) => {
                    this._isBusy = false;
                    if (arr.length && null != this._timerId) {
                        const renderingIsDone = arr[0];
                        const renderingProgress = arr[1] * 100;
                        const remainingTimeMilliseconds = arr[2];
                        const now = new Date().getTime();
                        $('#backgroundImg').attr('src', this._sessionId + '/image.png?' + now);

                        $("#progressBar").progressbar("value", renderingProgress);

                        const root = this._viewer.model.getAbsoluteRootNode();
                        const ratio = 5;
                        if (ratio >= renderingProgress) {

                            if (this._isAutoSlide) {
                                const opacity = (ratio - renderingProgress) / ratio;
                                this._viewer.model.setNodesOpacity([root], opacity);
                                $("#opacitySlider").slider("value",opacity);
                            }
                        }
                        else {
                            if (this._isAutoSlide) {
                                this._viewer.model.setNodesOpacity([root], 0);
                                $("#opacitySlider").slider("value",0);
                            }
                        }

                        if (100 == renderingProgress) {
                            this._clearRaytracing();
                            $('#progress').hide();
                            $('[data-command="Raytracing"]').data("on", false).css("background-color", "gainsboro");
                        }
                    }
                });
            }
        }, 200);
    }

    _clearRaytracing() {
        if (null != this._timerId) {
            clearInterval(this._timerId);
            this._timerId = null;
            this._isBusy = false;
            $("#loadingImage").hide();
        }
    }

    setMaterial(name) {
        const preserveColor = Number($("#checkPreserveColor").prop('checked'));
        const overrideMaterial = Number($("#checkOverrideMaterial").prop('checked'));

        const params = {
            nodeName: name,
            redFile: this._currentMaterialName,
            preserveColor: preserveColor,
            overrideMaterial: overrideMaterial
        }

        this._serverCaller.CallServerPost("SetMaterial", params).then((arr) => {
            this._viewer.model.resetModelHighlight();
            this._isAutoSlide = true;
        });
    }

    _downloadImage(from, to) {
        let oReq = new XMLHttpRequest(),
            a = document.createElement('a'), file;
        let versioningNum = new Date().getTime()
        oReq.open('GET', from + "?" + versioningNum, true);
        oReq.responseType = 'blob';
        oReq.onload = (oEvent) => {
            var blob = oReq.response;
            if (window.navigator.msSaveBlob) {
                // IE or Edge
                window.navigator.msSaveBlob(blob, filename);
            }
            else {
                // Other
                var objectURL = window.URL.createObjectURL(blob);
                var link = document.createElement("a");
                document.body.appendChild(link);
                link.href = objectURL;
                link.download = to;
                link.click();
                document.body.removeChild(link);
            }
            // Delete download source file in server
            const params = {};
            this._serverCaller.CallServerPost("DownloadImage", params);
            $("#loadingImage").hide();
        };
        oReq.send();
    }

    async invokeCreateFloor(params) {
        // Create HC mesh
        if (null != this._floorMeshId) {
            await this._viewer.model.deleteMeshInstances([this._floorMeshId]);
        }

        let vertexData = [];
        let normalData = [];
        for (let id of params.faceList) {
            vertexData.push(params.points[id * 3 + 0]);
            vertexData.push(params.points[id * 3 + 1]);
            vertexData.push(params.points[id * 3 + 2]);
            normalData.push(0.0);
            normalData.push(0.0);
            normalData.push(1.0);
        }
        const meshData = new Communicator.MeshData();
        meshData.setFaceWinding(Communicator.FaceWinding.CounterClockwise);
        meshData.addFaces(vertexData, normalData);
        const meshId = await this._viewer.model.createMesh(meshData);

        const r = this._floorColor[0];
        const g = this._floorColor[1];
        const b = this._floorColor[2];
        const faceColor = new Communicator.Color(r, g, b);
        const meshInstanceData = new Communicator.MeshInstanceData(meshId, undefined, "HL_floorPlane", faceColor);
        this._floorMeshId = await this._viewer.model.createMeshInstance(meshInstanceData);
        const opacity = 1 - $('#colorA').val();
        this._viewer.model.setNodesOpacity([this._floorMeshId], opacity);

        await this._serverCaller.CallServerPost("AddFloorMesh", params);
        this._updateFloorMaterial(null);
    }

    async _deleteFloor() {
        if (null != this._floorMeshId) {
            this._createFloorOp.Delete();
            $('#addFloorDlg').dialog('close');

            await this._viewer.model.deleteNode( this._floorMeshId);
            this._floorMeshId = null;

            await this._serverCaller.CallServerPost("DeleteFloorMesh", {});
            this._setDefaultOperators();
        }
    }

    async _updateFloorMaterial(formData) {
        if (null != formData) {
            const arr = await this._serverCaller.CallServerSubmitFile(formData);
        }

        const r = this._floorColor[0];
        const g = this._floorColor[1];
        const b = this._floorColor[2];
        const faceColor = new Communicator.Color(r, g, b);
        await this._viewer.model.setNodesFaceColor([this._floorMeshId], faceColor);
        const opacity = 1 - $('#colorA').val();
        this._viewer.model.setNodesOpacity([this._floorMeshId], opacity);
        const textureScale = $('#textureScale').val();

        const color = [
            this._floorColor[0] / 255,
            this._floorColor[1] / 255,
            this._floorColor[2] / 255,
            $('#colorA').val()
        ]
        const params = {
            color: color,
            textureScale: textureScale
        };
        this._serverCaller.CallServerPost("UpdateFloorMaterial", params);
    }

    async _changeUpVector() {
        const upVect = $('#upVector').val();
        const rotAng = $('#rotationAng').val();
        
        const root = this._viewer.model.getAbsoluteRootNode();
        const nodes = this._viewer.model.getNodeChildren(root);

        // Reset visibility
        this._clearRaytracing();
        $('#backgroundImg').attr('src', 'css/images/default_background.png');
        await this._viewer.model.resetNodesOpacity([root]);

        let roMatrix = new Communicator.Matrix();
        switch (upVect) {
        case "X": roMatrix = Communicator.Matrix.yAxisRotation(90); break;
        case "Y": roMatrix = Communicator.Matrix.xAxisRotation(-90); break;
        case "Z": break;
        case "-X": roMatrix = Communicator.Matrix.yAxisRotation(-90); break;
        case "-Y": roMatrix = Communicator.Matrix.xAxisRotation(90); break;
        case "-Z": roMatrix = Communicator.Matrix.xAxisRotation(180);break;
        }

        const zRoMatrix = Communicator.Matrix.zAxisRotation(-rotAng);
        roMatrix = Communicator.Matrix.multiply(roMatrix, zRoMatrix);

        // Compute rotation center
        if (undefined == this._rotationCenter) {
            const box = await this._viewer.model.getModelBounding(true, false);
            this._rotationCenter = box.min.copy().add(box.max.copy().subtract(box.min).scale(0.5));
        }

        // Compute center point after rotation
        const roPoint = Communicator.Point3.zero();
        roMatrix.transform(this._rotationCenter, roPoint);

        // Create translation matrix to shift the node arond rotation center after rotation
        const trMatrix = new Communicator.Matrix();
        trMatrix.setTranslationComponent(this._rotationCenter.x - roPoint.x, this._rotationCenter.y - roPoint.y, this._rotationCenter.z - roPoint.z);

        // Compute the node matrix of after rotation (multiplyMatrix * translationMatrix)
        const matrix = Communicator.Matrix.multiply(roMatrix, trMatrix);
        
        // set matrix
        for (let node of nodes) {
            if (node != this._floorMeshId)
                await this._viewer.model.setNodeMatrix(node, matrix);
        }

        let matArr = [];
        for (let i = 0; i < 16; i++) {
            matArr.push(matrix.m[i]);
        }
        const params = {
            matrix: matArr
        }

        await this._serverCaller.CallServerPost("SetRootTransform", params);

        this._invokeDraw();
    }
}
export {
    Main
}