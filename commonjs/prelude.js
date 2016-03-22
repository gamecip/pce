var Module = Module || {};

// hide node/commonjs globals so emscripten doesn't get confused
var process = null; 
var require = null; 

var pathGetFilenameRegex = /\/([^\/]+)$/;

function pathGetFilename(path) {
  var matches = path.match(pathGetFilenameRegex);
  if (matches && matches.length) {
    return matches[1];
  } else {
    return path;
  }
}

function addAutoloader(module) {
  var loadDatafiles = function() {
    module.autoloadFiles.forEach(function(filepath) {
      module.FS_createPreloadedFile('/', pathGetFilename(filepath), filepath, true, true);
    });
  };

  if (module.autoloadFiles) {
    module.preRun = module.preRun || [];
    module.preRun.unshift(loadDatafiles);
  }

  return module;
}

// inject extra behaviours
addAutoloader(Module);

window._SDL_CreateRGBSurfaceFrom = function(pixels, width, height, depth, pitch, rmask, gmask, bmask, amask) {
  // TODO: Take into account depth and pitch parameters.

  var surface = SDL.makeSurface(width, height, 0, false, 'CreateRGBSurfaceFrom', rmask, gmask, bmask, amask);

  var surfaceData = SDL.surfaces[surface];
  var surfaceImageData = surfaceData.ctx.getImageData(0, 0, width, height);
  var surfacePixelData = surfaceImageData.data;

  // Fill pixel data to created surface.
  // Supports SDL_PIXELFORMAT_RGBA8888 and SDL_PIXELFORMAT_RGB888
  var channels = amask ? 4 : 3; // RGBA8888 or RGB888
  for (var pixelOffset = 0; pixelOffset < width*height; pixelOffset++) {
    surfacePixelData[pixelOffset*4+0] = HEAPU8[pixels + (pixelOffset*channels+0)]; // R
    surfacePixelData[pixelOffset*4+1] = HEAPU8[pixels + (pixelOffset*channels+1)]; // G
    surfacePixelData[pixelOffset*4+2] = HEAPU8[pixels + (pixelOffset*channels+2)]; // B
    surfacePixelData[pixelOffset*4+3] = amask ? HEAPU8[pixels + (pixelOffset*channels+3)] : 0xff; // A
  };

  surfaceData.ctx.putImageData(surfaceImageData, 0, 0);

  return surface;
};

Module.preRun = Module.preRun || [];
