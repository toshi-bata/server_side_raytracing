class SetMaterialOperator {
    constructor(viewer, owner) {
        this._viewer = viewer;
        this._owner = owner;
        this._ptFirst = null;
    }

    onMouseDown(event) {
        this._ptFirst = event.getPosition();
    }

    onMouseUp(event) {
        if (null == this._ptFirst) return;

        const ptCurrent = event.getPosition();
        const pointDistance = Communicator.Point2.subtract(this._ptFirst, ptCurrent).length();
        this._ptFirst = null;

        if (5 > pointDistance && event.getButton() == Communicator.Button.Left) {
            const pickConfig = new Communicator.PickConfig(Communicator.SelectionMask.Face);
            this._viewer.view.pickFromPoint(event.getPosition(), pickConfig).then((selectionItem) => {
                const nodeId = selectionItem.getNodeId();
                if (null != nodeId) {
                    const root = this._viewer.model.getAbsoluteRootNode();
                    this._viewer.model.resetNodesOpacity([root]);
                    
                    this._viewer.model.setNodesHighlighted([nodeId], true);

                    let name = this._viewer.model.getNodeName(nodeId);
                    if ("HL_floorPlane" != name) {
                        const parentId = this._viewer.model.getNodeParent(nodeId);
                        name = this._viewer.model.getNodeName(parentId);
                    }
                    this._owner.setMaterial(name);

                    event.setHandled(true);
                }
            });
        }
    }
}