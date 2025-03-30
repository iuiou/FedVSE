/**
 * Created by 30947 on 2018/7/18.
 */
// import * as echarts from 'echarts';

const { color } = require("echarts");

$(function(){

    char1();
    char2();
    char3();
    char4();

})

//统计分析图
function char1() {

    var myChart = echarts.init($("#char1")[0]);

    option = {
        legend: {
          top: 'bottom'
        },
        // toolbox: {
        //   show: true,
        //   feature: {
        //     mark: { show: true },
        //     dataView: { show: true, readOnly: false },
        //     restore: { show: true },
        //     saveAsImage: { show: true }
        //   }
        // },
        legend: {
                  orient : 'vertical',
                  x : 'right',
                  textStyle : {
                      color : '#ffffff',
      
                  },
                  data:['Meituan','Caocao', 'DiDi','T3go','Amap']
              },
        series: [
          {
            name: 'Nightingale Chart',
            type: 'pie',
            radius: [16, 80],
            center: ['42%', '50%'],
            roseType: 'area',
            itemStyle: {
              borderRadius: 5
            },
            data: [
                {value:234, name:'Meituan'},
                {value:135, name:'Caocao'},
                {value:335, name:'DiDi'},
                {value:310, name:'T3go'},
                {value:270, name:'Amap'},
            ]
          }
        ]
      };

    myChart.setOption(option);
    window.addEventListener('resize', function () {myChart.resize();})

}

// function char1() {
//     var myChart = echarts.init($("#char1")[0]);
//     option = {
//         legend: {
//           top: 'bottom'
//         },
//         toolbox: {
//           show: true,
//           feature: {
//             mark: { show: true },
//             dataView: { show: true, readOnly: false },
//             restore: { show: true },
//             saveAsImage: { show: true }
//           }
//         },
//         series: [
//           {
//             name: 'Nightingale Chart',
//             type: 'pie',
//             radius: [50, 250],
//             center: ['50%', '50%'],
//             roseType: 'area',
//             itemStyle: {
//               borderRadius: 8
//             },
//             data: [
//               { value: 40, name: 'rose 1' },
//               { value: 38, name: 'rose 2' },
//               { value: 32, name: 'rose 3' },
//               { value: 30, name: 'rose 4' },
//               { value: 28, name: 'rose 5' },
//               { value: 26, name: 'rose 6' },
//               { value: 22, name: 'rose 7' },
//               { value: 18, name: 'rose 8' }
//             ]
//           }
//         ]
//       };
//     myChart.setOption(option);
//     window.addEventListener('resize', function () {myChart.resize();})
// }

// function char2() {

//     var myChart = echarts.init($("#char2")[0]);

//     option = {
//         tooltip : {
//             trigger: 'axis',
//             axisPointer : {            // 坐标轴指示器，坐标轴触发有效
//                 type : 'shadow'        // 默认为直线，可选为：'line' | 'shadow'
//             }
//         },
//         grid: {show:'true',borderWidth:'0'},
//         legend: {
//             data:['行驶', '停车','熄火','离线'],
//             textStyle : {
//                 color : '#ffffff',

//             }
//         },

//         calculable :false,
//         xAxis : [
//             {
//                 type : 'value',
//                 axisLabel: {
//                     show: true,
//                     textStyle: {
//                         color: '#fff'
//                     }
//                 },
//                 splitLine:{
//                     lineStyle:{
//                         color:['#f2f2f2'],
//                         width:0,
//                         type:'solid'
//                     }
//                 }

//             }
//         ],
//         yAxis : [
//             {
//                 type : 'category',
//                 data : ['客运车','危险品车','网约车','学生校车'],
//                 axisLabel: {
//                     show: true,
//                     textStyle: {
//                         color: '#fff'
//                     }
//                 },
//                 splitLine:{
//                     lineStyle:{
//                         width:0,
//                         type:'solid'
//                     }
//                 }
//             }
//         ],
//         series : [
//             {
//                 name:'行驶',
//                 type:'bar',
//                 stack: '总量',
//                 itemStyle : { normal: {label : {show: true, position: 'insideRight'}}},
//                 data:[320, 302, 301, 334]
//             },
//             {
//                 name:'停车',
//                 type:'bar',
//                 stack: '总量',
//                 itemStyle : { normal: {label : {show: true, position: 'insideRight'}}},
//                 data:[120, 132, 101, 134]
//             },
//             {
//                 name:'熄火',
//                 type:'bar',
//                 stack: '总量',
//                 itemStyle : { normal: {label : {show: true, position: 'insideRight'}}},
//                 data:[220, 182, 191, 234]
//             },
//             {
//                 name:'离线',
//                 type:'bar',
//                 stack: '总量',
//                 itemStyle : { normal: {label : {show: true, position: 'insideRight'}}},
//                 data:[150, 212, 201, 154]
//             }

//         ]
//     };

//     myChart.setOption(option);
//     window.addEventListener('resize', function () {myChart.resize();})
//     myChart.on('click',  function(param) {
//         alert("更多模板，关注公众号【DreamCoders】\n回复'BigDataView'即可获取\n或前往Gitee下载 https://gitee.com/iGaoWei/big-data-view")
//         setTimeout(function(){
//             location.href = "https://gitee.com/iGaoWei/big-data-view";
//         },20000);
//     });


// }



function char2() {
    var myChart = echarts.init($("#char2")[0]);

    option = {
        color: ['#80FFA5', '#00DDFF', '#37A2FF', '#FF0087', '#FFBF00'],
        tooltip: {
          trigger: 'axis',
          axisPointer: {
            type: 'cross',
            label: {
              backgroundColor: '#6a7985'
            }
          }
        },
        legend: {
          data: ['Line 1', 'Line 2', 'Line 3', 'Line 4', 'Line 5']
        },
        grid: {
          left: '3%',
          right: '4%',
          bottom: '3%',
          containLabel: true
        },
        xAxis: [
          {
            type: 'category',
            boundaryGap: false,
            data: ['Mon', 'Tues', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
          }
        ],
        yAxis: [
          {
            type: 'value'
          }
        ],
        series: [
          {
            name: 'Line 1',
            type: 'line',
            stack: 'Total',
            smooth: true,
            lineStyle: {
              width: 0
            },
            showSymbol: false,
            areaStyle: {
              opacity: 0.8,
              color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                {
                  offset: 0,
                  color: 'rgb(128, 255, 165)'
                },
                {
                  offset: 1,
                  color: 'rgb(1, 191, 236)'
                }
              ])
            },
            emphasis: {
              focus: 'series'
            },
            data: [140, 232, 101, 264, 90, 340, 250]
          },
          {
            name: 'Line 2',
            type: 'line',
            stack: 'Total',
            smooth: true,
            lineStyle: {
              width: 0
            },
            showSymbol: false,
            areaStyle: {
              opacity: 0.8,
              color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                {
                  offset: 0,
                  color: 'rgb(0, 221, 255)'
                },
                {
                  offset: 1,
                  color: 'rgb(77, 119, 255)'
                }
              ])
            },
            emphasis: {
              focus: 'series'
            },
            data: [120, 282, 111, 234, 220, 340, 310]
          },
          {
            name: 'Line 3',
            type: 'line',
            stack: 'Total',
            smooth: true,
            lineStyle: {
              width: 0
            },
            showSymbol: false,
            areaStyle: {
              opacity: 0.8,
              color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                {
                  offset: 0,
                  color: 'rgb(55, 162, 255)'
                },
                {
                  offset: 1,
                  color: 'rgb(116, 21, 219)'
                }
              ])
            },
            emphasis: {
              focus: 'series'
            },
            data: [320, 132, 201, 334, 190, 130, 220]
          },
          {
            name: 'Line 4',
            type: 'line',
            stack: 'Total',
            smooth: true,
            lineStyle: {
              width: 0
            },
            showSymbol: false,
            areaStyle: {
              opacity: 0.8,
              color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                {
                  offset: 0,
                  color: 'rgb(255, 0, 135)'
                },
                {
                  offset: 1,
                  color: 'rgb(135, 0, 157)'
                }
              ])
            },
            emphasis: {
              focus: 'series'
            },
            data: [220, 402, 231, 134, 190, 230, 120]
          },
          {
            name: 'Line 5',
            type: 'line',
            stack: 'Total',
            smooth: true,
            lineStyle: {
              width: 0
            },
            showSymbol: false,
            label: {
              show: true,
              position: 'top'
            },
            areaStyle: {
              opacity: 0.8,
              color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                {
                  offset: 0,
                  color: 'rgb(255, 191, 0)'
                },
                {
                  offset: 1,
                  color: 'rgb(224, 62, 76)'
                }
              ])
            },
            emphasis: {
              focus: 'series'
            },
            data: [220, 302, 181, 234, 210, 290, 150]
          }
        ]
      };

    myChart.setOption(option);
    window.addEventListener('resize', function () {myChart.resize();})
    myChart.on('click',  function(param) {
        setTimeout(function(){
            location.href = "https://gitee.com/iGaoWei/big-data-view";
        },20000);
    });
}


function char3() {

    var myChart = echarts.init($("#char3")[0]);

    option = {
        legend: {
            data:['车辆行驶数量'],
            textStyle : {
                color : '#ffffff',

            }
        },
        grid: {show:'true',borderWidth:'0'},

        calculable : false,
        tooltip : {
            trigger: 'axis',
            formatter: "Temperature : <br/>{b}km : {c}°C"
        },
        xAxis : [
            {
                type : 'value',
                axisLabel : {
                    formatter: '{value}',
                    textStyle: {
                        color: '#fff'
                    }
                },

                splitLine:{
                    lineStyle:{
                        width:0,
                        type:'solid'
                    }
                }
            }
        ],
        yAxis : [
            {
                type : 'category',
                axisLine : {onZero: false},
                axisLabel : {
                    formatter: '{value} km',
                    textStyle: {
                        color: '#fff'
                    }
                },
                splitLine:{
                    lineStyle:{
                        width:0,
                        type:'solid'
                    }
                },
                boundaryGap : false,
                data : ['0', '10', '20', '30', '40', '50', '60', '70', '80']
            }
        ],
        series : [
            {
                name:'车辆行驶数量',
                type:'line',
                smooth:true,
                itemStyle: {
                    normal: {
                        lineStyle: {
                            shadowColor : 'rgba(0,0,0,0.4)'
                        }
                    }
                },
                data:[15, 0, 20, 45, 22.1, 25, 70, 55, 76]
            }
        ]
    };

    myChart.setOption(option);
    window.addEventListener('resize', function () {myChart.resize();})
    myChart.on('click',  function(param) {
        setTimeout(function(){
            location.href = "https://gitee.com/iGaoWei/big-data-view";
        },20000);
    });
}
function char4() {

    var myChart = echarts.init($("#char4")[0]);

    option = {
        grid: {show:'true',borderWidth:'0'},
        tooltip : {
            trigger: 'axis',
            axisPointer : {            // 坐标轴指示器，坐标轴触发有效
                type : 'shadow'        // 默认为直线，可选为：'line' | 'shadow'
            },

            formatter: function (params) {
                var tar = params[0];
                return tar.name + '<br/>' + tar.seriesName + ' : ' + tar.value;
            }
        },

        xAxis : [
            {
                type : 'category',
                splitLine: {show:false},
                data : ['客运车','危险品车','网约车','学生校车'],
                axisLabel: {
                    show: true,
                    textStyle: {
                        color: '#fff'
                    }
                }

            }
        ],
        yAxis : [
            {
                type : 'value',
                splitLine: {show:false},
                axisLabel: {
                    show: true,
                    textStyle: {
                        color: '#fff'
                    }
                }
            }
        ],
        series : [

            {
                name:'报警数量',
                type:'bar',
                stack: '总量',
                itemStyle : { normal: {label : {show: true, position: 'inside'}}},
                data:[2900, 1200, 300, 200, 900, 300]
            }
        ]
    };

    myChart.setOption(option);
    window.addEventListener('resize', function () {myChart.resize();})
    myChart.on('click',  function(param) {
        setTimeout(function(){
            location.href = "https://gitee.com/iGaoWei/big-data-view";
        },20000);
    });
}
