extern "C" {
    unsigned char* assimp_stbi_load_from_memory(unsigned char const* buffer, int len, int* x, int* y, int* channels_in_file, int desired_channels);
    unsigned char* assimp_stbi_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
    void assimp_stbi_image_free(void* retval_from_stbi_load);
    char const* assimp_stbi_failure_reason(void);
    
    unsigned char* stbi_load_from_memory(unsigned char const* buffer, int len, int* x, int* y, int* channels_in_file, int desired_channels) {
        return assimp_stbi_load_from_memory(buffer, len, x, y, channels_in_file, desired_channels);
    }
    
    unsigned char* stbi_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels) {
        return assimp_stbi_load(filename, x, y, channels_in_file, desired_channels);
    }
    
    void stbi_image_free(void* retval_from_stbi_load) {
        assimp_stbi_image_free(retval_from_stbi_load);
    }
    
    char const* stbi_failure_reason(void) {
        return assimp_stbi_failure_reason();
    }
}
