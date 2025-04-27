#extension GL_EXT_nonuniform_qualifier : enable

#define HLS_UNIFORM_BUFFER_BINDING_INDEX 0
#define HLS_STORAGE_BUFFER_BINDING_INDEX 1
#define HLS_TEXTURE_BINDING_INDEX 2

#define HLS_REGISTER_UNIFORM_BUFFER(Name, Struct)                              \
    layout(set = 0, binding = HLS_UNIFORM_BUFFER_BINDING_INDEX)                \
        uniform Name Struct g##Name##s[];

#define HLS_REGISTER_STORAGE_BUFFER(Name, Struct)                              \
    struct Name Struct;                                                        \
    layout(set = 0, binding = HLS_STORAGE_BUFFER_BINDING_INDEX, std430)        \
        readonly buffer Name##Buffer                                           \
    {                                                                          \
        Name items[];                                                          \
    }                                                                          \
    g##Name##s[];

#define HLS_REGISTER_TEXTURE_BUFFER(Name, Type)                                \
    layout(set = 0, binding = HLS_TEXTURE_BINDING_INDEX)                       \
        uniform Type g##Name##s[];

#define HLS_ACCESS_UNIFORM_BUFFER(BufferName, Index, ElementName)              \
    g##BufferName##s[Index].ElementName

#define HLS_ACCESS_STORAGE_BUFFER(BufferName, Index)                           \
    g##BufferName##s[Index].items

#define HLS_ACCESS_TEXTURE_BUFFER(BufferName, Index) g##BufferName##s[Index]
