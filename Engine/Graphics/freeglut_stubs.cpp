// Stubs pour les fonctions spaceball de FreeGLUT non implémentées sur Windows
extern "C" {
    // Stub pour XParseGeometry
    int XParseGeometry(const char* /*parsestring*/, int* /*x*/, int* /*y*/, 
                       unsigned int* /*width*/, unsigned int* /*height*/) {
        return 0;
    }
    
    // Stubs pour spaceball (périphérique 3D non utilisé)
    void fgSpaceballHandleWinEvent(void* /*msg*/) {}
    
    void fgPlatformInitializeSpaceball(void) {}
    
    void fgPlatformSpaceballClose(void) {}
    
    int fgPlatformHasSpaceball(void) {
        return 0; // Pas de spaceball
    }
    
    int fgPlatformSpaceballNumButtons(void) {
        return 0;
    }
    
    void fgPlatformSpaceballSetWindow(void* /*window*/) {}
}
