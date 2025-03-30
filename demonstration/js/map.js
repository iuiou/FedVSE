/*大屏*/
$(function(){


    initMap();





})
//地图界面高度设置



//加载地图
function initMap() {
    const map = new AMap.Map('map_div', {
        zoom: 11,
        center: [116.404, 39.915],  // 北京坐标（GCJ-02坐标系）[[5]]
        // mapStyle: 'amap://styles/midnight',  // 夜间主题样式 [[9]]
        features: ['bg','point','road','building'],  // 显示要素
        lang: "en" //可选值：en，zh_en, zh_cn
    });

    map.setStatus({
        showIndoorMap: false, // 是否在有矢量底图的时候自动展示室内地图，PC默认true,移动端默认false
        resizeEnable: true, //是否监控地图容器尺寸变化，默认值为false
        dragEnable: true, // 地图是否可通过鼠标拖拽平移，默认为true
        keyboardEnable: false,//地图是否可通过键盘控制，默认为true
        doubleClickZoom: true,// 地图是否可通过双击鼠标放大地图，默认为true
        zoomEnable: true, //地图是否可缩放，默认值为true
        rotateEnable: true// 地图是否可旋转，3D视图默认为true，2D视图默认false
      });

    // 添加25个随机标记 [[7]]
    // const bounds = map.getBounds();
    // const sw = bounds.getSouthWest();
    // const ne = bounds.getNorthEast();
    // const lngSpan = Math.abs(sw.lng - ne.lng);
    // const latSpan = Math.abs(ne.lat - sw.lat);

    // for (let i = 0; i < 25; i++) {
    //     const marker = new AMap.Marker({
    //         position: [
    //             sw.lng + lngSpan * Math.random() * 0.7,
    //             ne.lat - latSpan * Math.random() * 0.7
    //         ],
    //         map: map
    //     });
    // }

    // 添加地图类型控件 [[9]]
    // AMap.plugin(['AMap.MapType'], () => {
    //     const mapTypeControl = new AMap.MapType({
    //         defaultType: 0,  // 默认普通地图
    //         showRoad: true   // 显示路况
    //     });
    //     map.addControl(mapTypeControl);
    // });

    // 添加城市选择控件（需自定义实现）[[3]][[9]]
    // AMap.plugin(['AMap.ToolBar'], () => {
    //     const toolBar = new AMap.ToolBar({
    //         position: 'LT',  // 左上角
    //         offset: [10, 50]
    //     });
    //     map.addControl(toolBar);
    // });

}
