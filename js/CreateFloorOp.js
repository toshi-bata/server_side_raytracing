class CreateFloorOperator {
    constructor(viewer, owner) {
        this._viewer = viewer;
        this._owner = owner;
        this._vertices = [];
        this._markup;
        this._markupHandle = null;
        this._activeID;
        this._anchorPlane;
    }

    _createFloor() {
        let points = [];
        for (let vertex of this._vertices) {
            points.push(vertex.x);
            points.push(vertex.y);
            points.push(vertex.z);
        }

        const faceList = [0, 1, 2, 0, 2, 3];

        const params = {
            pointCnt: points.length,
            points: points,
            faceCnt: 2,
            faceList: faceList
        }
        this._owner.invokeCreateFloor(params);
    }

    Init() {
        this._activeID = -1;

        this._viewer.model.getModelBounding(true, false).then((box) => {
            this._vertices.push(box.min);
            this._vertices.push(new Communicator.Point3(box.max.x, box.min.y, box.min.z));
            this._vertices.push(new Communicator.Point3(box.max.x, box.max.y, box.min.z));
            this._vertices.push(new Communicator.Point3(box.min.x, box.max.y, box.min.z));

            this._createFloor();

            this._anchorPlane = Communicator.Plane.createFromPointAndNormal(box.min, new Communicator.Point3(0, 0, 1));

            this._markup = new FloorMarkup(this._viewer, this._vertices);
            this._markupHandle = this._viewer.markupManager.registerMarkup(this._markup);
        });
    }

    Term() {
        if (null != this._markupHandle) {
            this._viewer.markupManager.unregisterMarkup(this._markupHandle);
            this._markupHandle = null;
        }
    }

    onMouseDown(event) {
        if (Communicator.Button.Left != event.getButton()) return;

        var selectPoint = event.getPosition();
        const markup = this._viewer.markupManager.pickMarkupItem(selectPoint);
        if (markup) {
            this._activeID = markup.GetSelectedId();
            if (0 <= this._activeID) {
                event.setHandled(true);
            }
        }
    }

    onMouseMove(event) {
        if (0 <= this._activeID) {
            const raycast = this._viewer.view.raycastFromPoint(event.getPosition());
            const vertex = Communicator.Point3.zero();
            if (this._anchorPlane.intersectsRay(raycast, vertex)) {
                this._vertices[this._activeID] = vertex;
                this._markup.SetVertex(this._activeID, vertex);
            }
            event.setHandled(true);
        }
    }

    onMouseUp(event) {
        if (0 <= this._activeID) {
            this._createFloor();
            event.setHandled(true);
        }
        this._activeID = -1;
    }
}

class FloorMarkup extends Communicator.Markup.MarkupItem {
    constructor(viewer, vertices) {
        super();
        this._viewer = viewer;
        this._vertices = vertices;
        this._cornerCirArr = [];
        this._rad = 4;
        this._selectedId = -1;

        for (let vertex of vertices) {
            const cir = new Communicator.Markup.Shape.Circle();
            cir.setRadius(this._rad);
            cir.setFillOpacity(0);
            cir.setStrokeColor(new Communicator.Color(255, 0, 0));
            cir.setStrokeWidth(2);

            this._cornerCirArr.push(cir);
        }
    }

    draw() {
        const renderer = this._viewer.markupManager.getRenderer();
        for (let i = 0; i < this._vertices.length; i++) {
            const pnt3d = this._viewer.view.projectPoint(this._vertices[i]);
            const pnt2d = Communicator.Point2.fromPoint3(pnt3d);
            this._cornerCirArr[i].setCenter(pnt2d);

            renderer.drawCircle(this._cornerCirArr[i]);
        }
    }

    hit(point) {
        for (let i = 0; i < this._cornerCirArr.length; i++) {
            const cent = this._cornerCirArr[i].getCenter();
            const dist = Communicator.Point2.distance(cent, point);
            if ((this._rad + 1) * 2 >= dist){
            this._selectedId = i;
            return true;
            }
        }
        this._selectedId = -1;
        return false
    }

    remove() {
        return;
    }

    GetSelectedId() {
        return this._selectedId;
    }

    SetVertex(id, vertex) {
        this._vertices[id] = vertex;
        this._viewer.markupManager.refreshMarkup();
    }
}