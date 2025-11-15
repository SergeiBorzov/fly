#extension GL_EXT_nonuniform_qualifier : require

#define FLY_UNIFORM_BUFFER_BINDING_INDEX 0
#define FLY_STORAGE_BUFFER_BINDING_INDEX 1
#define FLY_TEXTURE_BINDING_INDEX 2
#define FLY_STORAGE_TEXTURE_BINDING_INDEX 3
#define FLY_ACCELERATION_STRUCTURE_BINDING_INDEX 4

#define FLY_REGISTER_UNIFORM_BUFFER(Name, Struct)                              \
    layout(set = 0, binding = FLY_UNIFORM_BUFFER_BINDING_INDEX)                \
        uniform Name Struct g##Name##s[];

#define FLY_REGISTER_STORAGE_BUFFER(Access, Name, Struct)                      \
    struct Name Struct;                                                        \
    layout(set = 0, binding = FLY_STORAGE_BUFFER_BINDING_INDEX, std430)        \
        Access buffer Name##Buffer                                             \
    {                                                                          \
        Name items[];                                                          \
    }                                                                          \
    g##Name##s[];

#define FLY_REGISTER_TEXTURE_BUFFER(Name, Type)                                \
    layout(set = 0, binding = FLY_TEXTURE_BINDING_INDEX)                       \
        uniform Type g##Name##s[];

#define FLY_REGISTER_STORAGE_TEXTURE_BUFFER(Access, Name, Type, Format)        \
    layout(set = 0, binding = FLY_STORAGE_TEXTURE_BINDING_INDEX, Format)       \
        uniform Access Type g##Name##s[];

#define FLY_REGISTER_ACCELERATION_STRUCTURE_BUFFER(Name)                       \
    layout(set = 0, binding = FLY_ACCELERATION_STRUCTURE_BINDING_INDEX)        \
        uniform accelerationStructureEXT g##Name##s[];

#define FLY_ACCESS_UNIFORM_BUFFER(BufferName, Index, ElementName)              \
    g##BufferName##s[Index].ElementName

#define FLY_ACCESS_STORAGE_BUFFER(BufferName, Index)                           \
    g##BufferName##s[Index].items

#define FLY_ACCESS_TEXTURE_BUFFER(BufferName, Index) g##BufferName##s[Index]

#define FLY_ACCESS_STORAGE_TEXTURE_BUFFER(BufferName, Index)                   \
    g##BufferName##s[Index]

#define FLY_ACCESS_ACCELERATION_STRUCTURE_BUFFER(BufferName, Index)            \
    g##BufferName##s[Index]
