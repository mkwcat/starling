#pragma once

class Filesystem {
public:
static s32 Open(const char* path, s32 flags, s32 mode);
static s32 Close(s32 fd);
static s32 Read(s32 fd, void* data, s32 size);
static s32 Write(s32 fd, const void* data, s32 size);
static s32 Seek(s32 fd, s32 offset, s32 whence);
static s32 Tell(s32 fd);
static s32 GetSize(s32 fd);
};