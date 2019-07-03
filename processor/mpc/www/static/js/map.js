var map = L.map('map', {
    center: [45.2539, -79.4535],
    zoom: 13
});

var watercolourTiles = L.tileLayer('https://stamen-tiles-{s}.a.ssl.fastly.net/watercolor/{z}/{x}/{y}.{ext}', {
    attribution: 'Map tiles by <a href="http://stamen.com">Stamen Design</a>, <a href="http://creativecommons.org/licenses/by/3.0">CC BY 3.0</a> &mdash; Map data &copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>',
    subdomains: 'abcd',
    minZoom: 1,
    maxZoom: 14,
    ext: 'jpg'
});
var natgeoTiles = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/NatGeo_World_Map/MapServer/tile/{z}/{y}/{x}', {
    attribution: 'Tiles &copy; Esri &mdash; National Geographic, Esri, DeLorme, NAVTEQ, UNEP-WCMC, USGS, NASA, ESA, METI, NRCAN, GEBCO, NOAA, iPC',
    maxZoom: 16
});

var offlineTiles = L.tileLayer($SCRIPT_ROOT+'/tile/{z}/{y}/{x}.{ext}', {
    attribution: 'Tiles &copy; Esri &mdash; National Geographic, Esri, DeLorme, NAVTEQ, UNEP-WCMC, USGS, NASA, ESA, METI, NRCAN, GEBCO, NOAA, iPC',
    maxZoom: 16,
    ext: 'jpg'
});

var lineStyle = {
    "color": "#3f3f3f",
    "weight": 2,
    "opacity": 0.7
};
var topo = new L.GeoJSON.AJAX("/static/skel.geojson", {
    onEachFeature: function (feature, layer) {
        layer.bindTooltip(String(feature.properties["DEPTH"] + "m"), {
            sticky: true,
            permanent: false
        });
    },
    style: lineStyle
});

var imageLayer = L.layerGroup()
function addPhotoGroup(lat,lon, template) {
    imageLayer.addLayer(L.marker([lat,lon]).bindPopup(template,{maxHeight:200, minWidth:325}));
}

$.get($SCRIPT_ROOT + '/gallery', {
}, function (data) {
    console.log(data)
    for (var i=0;i<data.length;i++) {
        addPhotoGroup(data[i]['lat'], data[i]['lon'], data[i]['popup']);
    }
});


var baseMaps = {
    "Watercolour": watercolourTiles,
    "Nat Geo": natgeoTiles,
    "Offline": offlineTiles
};

var overlayMaps = {
    "Images": imageLayer,
    "Topographic": topo
};

L.control.layers(baseMaps, overlayMaps).addTo(map);
natgeoTiles.addTo(map)
imageLayer.addTo(map)

var downloadTiles = function() {
    var coords = map.getCenter()
    run('save_map', null, vars={'lat':coords.lat, 'lon':coords.lng})
};
