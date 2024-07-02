class Main {
    constructor() {
        this._viewer;
        this._port;
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
        this._materialDB;
        this._currentMaterialName;
        this._currentMaterialId;
        this._currentLightingId;
        this._isAutoSlide;
    }

    start (port, viewerMode, modelName, reverseProxy) {
        this._port = port;
        this._reverseProxy = reverseProxy;
        this._timerId = null;
        this._isBusy = false;
        this._requestServerProcess();
        this._createViewer(viewerMode, modelName);
        this._initEvents();
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
                timeout: () => {
                    this._serverCaller.CallServerPost("Clear");
                    console.log("Timeout");
                }
            });

            this._setMaterialOp = new SetMaterialOperator(this._viewer, this);
            this._setMaterialOpHandle = this._viewer.registerCustomOperator(this._setMaterialOp);
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

        // Tolerance select
        $('#sewingTol').val(0.01);
        $('#classifyTol').val(0.0001);

        // Prepare Material dialog
        $("#materialsDlg").dialog({
            autoOpen: false,
            height: 550,
            width: 350,
            modal: false,
            title: "Materials",
            closeOnEscape: true,
            position: {my: "left top", at: "left bottom", of: "#toolbarGr"},
        });

        // Load material info from JSON file
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

        // Lighting thumbnail click event handler
        this._setLightingThumbClickHandler();

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
            height: 200,
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
                case "Upload": {
                    $('#Upload3DDlg').dialog('open');
                    $('#cadFileSelect').click();
                } break;
                case "Raytracing": {
                    this._invokeRaytracing(command);
                } break;
                case "SetMaterial": {
                    if (!isOn) {
                        $('#materialsDlg').dialog('open');
                        this._viewer.operatorManager.push(this._setMaterialOpHandle);
                    }
                    else {
                        $('#materialsDlg').dialog('close');
                        this._setDefaultOperators();
                    }
                } break;
                case "SetLighting": {
                    $('#lightingModeDlg').dialog('open');
                } break;
                case "UpVector": {
                    $('#upVectorDlg').dialog('open');
                } break;
                case "Download": {
                    const serverName = this._sessionId + ".png";
                    const downloadName = "image.png";
                    this._downloadImage(serverName, downloadName);

                } break;
                default: {} break;
            }
        });

        // Toggle button common
        $(".toggleBtn").on("click", (e) => {
            let isOn = $(e.currentTarget).data("on");
            if(isOn) {
                $(e.currentTarget).data("on", false).css("background-color", "gainsboro");
            } else {
                $(e.currentTarget).data("on", true).css("background-color", "khaki");
            }
        });

        // Normal button common
        $(".normalBtn").mousedown((e) => {
            $(e.currentTarget).data("on", true).css("background-color", "khaki");
        }).mouseup((e) => {
            $(e.currentTarget).data("on", false).css("background-color", "gainsboro");
        });

        // Toolbar
        $('[data-command="Raytracing"]').prop("disabled", true).css("background-color", "darkgrey");

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

        // Up vector
        $("#upVector").val("Z");
        $('#upVector').change((e) => {
            const upVect = $(e.currentTarget).val();
            const root = this._viewer.model.getAbsoluteRootNode();

            // reset modelling matrix
            this._viewer.model.resetNodeMatrixToInitial(root);

            let roMatrix = new Communicator.Matrix();
            switch (upVect) {
            case "X": roMatrix = Communicator.Matrix.yAxisRotation(90); break;
            case "Y": roMatrix = Communicator.Matrix.xAxisRotation(-90); break;
            case "Z": break;
            case "-X": roMatrix = Communicator.Matrix.yAxisRotation(-90); break;
            case "-Y": roMatrix = Communicator.Matrix.xAxisRotation(90); break;
            case "-Z": roMatrix = Communicator.Matrix.xAxisRotation(180);break;
            }

            // Compute rotation center
            this._viewer.model.getModelBounding(true, false).then((box) => {
                const center = box.min.copy().add(box.max.copy().subtract(box.min).scale(0.5));

                // Compute center point after rotation
                const roPoint = Communicator.Point3.zero();
                roMatrix.transform(center, roPoint);

                // Create translation matrix to shift the node arond rotation center after rotation
                const trMatrix = new Communicator.Matrix();
                trMatrix.setTranslationComponent(center.x - roPoint.x, center.y - roPoint.y, center.z - roPoint.z);

                // Compute the node matrix of after rotation (multiplyMatrix * translationMatrix)
                const matrix = Communicator.Matrix.multiply(roMatrix, trMatrix);

                // set root matrix
                this._viewer.model.setNodeMatrix(root, matrix);

                let matArr = [];
                for (let i = 0; i < 16; i++) {
                    matArr.push(matrix.m[i]);
                }
                const params = {
                    matrix: matArr
                }

                this._serverCaller.CallServerPost("SetRootTransform", params).then(() => {
                });
            });
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

    _loadModel(params, formData, scModelName) {
        $("#loadingImage").show();
        const now = new Date().getTime();
        $('#backgroundImg').attr('src', 'css/images/default_background.png');

        this._serverCaller.CallServerPost("Clear").then(() => {
            this._requestServerProcess();
            this._serverCaller.CallServerPost("SetOptions", params).then(() => {
                this._serverCaller.CallServerSubmitFile(formData).then((arr) => {
                    let dataArr = Array.from(arr);
                    let unit = dataArr.shift();
                    unit = unit.toFixed(1);
                    unit = this._units[unit];
                    const area = dataArr.shift();
                    const volume = dataArr.shift();
                    const COG = [dataArr.shift(), dataArr.shift(), dataArr.shift()];
                    const BB = [dataArr.shift(), dataArr.shift(), dataArr.shift()];

                    const partProps = '<ul><li>Unit: ' + unit + '</li>'
                    + '<li>Surface area: ' + area.toFixed(2) + '<br>'
                    + '<li>Volume: ' + volume.toFixed(2) + '</li>'
                    + '<li>Center of Gravity: X= ' + COG[0].toFixed(2) + ', Y= ' + COG[1].toFixed(2) + ', Z= ' + COG[2].toFixed(2) + '</li>'
                    + '<li>Bounding box: X= ' + BB[0].toFixed(2) + ', Y= ' + BB[1].toFixed(2) + ', Z= ' + BB[2].toFixed(2) + '</li></ul>'

                    $("#dataInfo").html(partProps);

                    this._viewer.model.switchToModel(this._sessionId + "/" + scModelName).then((nodes) => {
                        const camera = this._viewer.view.getCamera();
                        camera.setProjection(Communicator.Projection.Perspective);
                        this._viewer.view.setCamera(camera);

                        const params = this._getRenderingParams();
                        this._serverCaller.CallServerPost("PrepareRendering", params).then(() => {
                            $('[data-command="Raytracing"]').prop("disabled", false).css("background-color", "gainsboro");
                        });
                        $("#loadingImage").hide();
                    });
                });
            });
        });
    }

    _loadEnv(formData) {
        $("#loadingImage").show();

        this._serverCaller.CallServerSubmitFile(formData).then((arr) => {
            $("#loadingImage").hide();
            if (0 < arr.length) {
                if (1 == arr[0]) {
                    // Add env map thumbnail 
                    const image = new Image();
                    image.src = "Lighting/EnvMapThumb-" + this._sessionId + ".png";
                    let $ele = $('<div />', {class:'itemList_thumbnail lightingItem thumbnail-selected',title: 'Env Map'});
                    $($ele).append(image);
                    $($ele).append("Env Map");
                    $('#lightingList').append($ele);

                    this._currentLightingId = 4;

                    this._setLightingThumbClickHandler();
                }
            }
        });
    }

    _setLightingThumbClickHandler() {
        // Lighting thumbnail click event handler
        $('.itemList_thumbnail').on('click', (e) => {
            const cls = $(e.currentTarget).attr('class');
            if (cls.match('lightingItem')) {
                // Highlight selected thumbnail
                const id = $('.itemList_thumbnail' + '.lightingItem').index(e.currentTarget) + 1

                if (this._currentLightingId == id)
                    return;

                $(`.itemList_thumbnail:nth-child(${this._currentLightingId})` + '.lightingItem').removeClass('thumbnail-selected');

                const title = $(e.currentTarget).attr('title');
                if ('Load Environment Map' == title) {
                    $('#UploadEnvDlg').dialog('open');
                    $('#envFileSelect').click();
                }
                else {
                    this._currentLightingId = id;
                    $(`.itemList_thumbnail:nth-child(${this._currentLightingId})` + '.lightingItem').addClass('thumbnail-selected');

                    this._setLightingMode(this._currentLightingId - 1);
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
        if (null == this._timerId) {   
            $("#loadingImage").show();

            const params = this._getRenderingParams();

            this._serverCaller.CallServerPost(command, params).then(() => {
                $("#loadingImage").hide();
                this._invokeDraw();
            });
        }
        else {
            this._clearRaytracing();
        }
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
                        $('#backgroundImg').attr('src', this._sessionId + '.png?' + now);

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

    _setLightingMode(lightingId) {
        const params = {
            lightingId: lightingId
        }

        this._serverCaller.CallServerPost("SetLighting", params).then((arr) => {
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
}
