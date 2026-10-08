#pragma once
#include <cstdint>
#include <string>

namespace Offsets {
    inline std::string ClientVersion = "version-cec3ad5889b447cf";
    namespace BasePart {
        inline constexpr std::uintptr_t ClusterNode = 0x180;
        inline constexpr std::uintptr_t Color3 = 0x198;
        inline constexpr std::uintptr_t Primitive = 0x178;
        inline constexpr std::uintptr_t Transparency = 0x120;
    }
    namespace CachedItem {
        inline constexpr std::uintptr_t FileMeshData = 0x28;
    }
    namespace Camera {
        inline constexpr std::uintptr_t Position = 0xec;
        inline constexpr std::uintptr_t Rotation = 0xc8;
        inline constexpr std::uintptr_t Viewport = 0x28c;
        inline constexpr std::uintptr_t ViewportSize = 0x2cc;
    }
    namespace CharacterMesh {
        inline constexpr std::uintptr_t BodyPart = 0x138;
        inline constexpr std::uintptr_t MeshId = 0xe8;
    }
    namespace DataModel {
        inline constexpr std::uintptr_t PlaceId = 0x188;
    }
    namespace DeviceContextD3D11 {
        inline constexpr std::uintptr_t RealContextPtr = 0x18;
    }
    namespace DeviceD3D11Gfx {
        inline constexpr std::uintptr_t ContextObj = 0x1b0;
        inline constexpr std::uintptr_t DevicePtr = 0x178;
        inline constexpr std::uintptr_t SwapChainPtr = 0x80;
    }
    namespace FakeDataModel {
        inline constexpr std::uintptr_t Pointer = 0x8bcfd50;
        inline constexpr std::uintptr_t RealDataModel = 0x1f8;
    }
    namespace FastCluster {
        inline constexpr std::uintptr_t BindingSubobject = 0x90;
        inline constexpr std::uintptr_t EntityBegin = 0x48;
        inline constexpr std::uintptr_t VTableRva = 0x6d6fc88;
        inline constexpr std::uintptr_t VTableRvaSub = 0x6d6fb58;
    }
    namespace FastClusterBinding {
        inline constexpr std::uintptr_t Owner = 0x60;
        inline constexpr std::uintptr_t VTableRva = 0x6d70c18;
    }
    namespace FastClusterEntity {
        inline constexpr std::uintptr_t DecalMaterialPtr = 0x48;
        inline constexpr std::uintptr_t MaterialPtr = 0x20;
        inline constexpr std::uintptr_t VTableRva = 0x6d70ce8;
    }
    namespace GeometryD3D11 {
        inline constexpr std::uintptr_t VTableRva = 0x6d03990;
    }
    namespace Humanoid {
        inline constexpr std::uintptr_t Health = 0x180;
    }
    namespace Instance {
        inline constexpr std::uintptr_t ChildrenEnd = 0x8;
        inline constexpr std::uintptr_t ChildrenStart = 0x78;
        inline constexpr std::uintptr_t ChildrenStride = 0x10;
        inline constexpr std::uintptr_t ClassDescriptor = 0x18;
        inline constexpr std::uintptr_t ClassName = 0x8;
        inline constexpr std::uintptr_t Name = 0x8;
        inline constexpr std::uintptr_t NameContainer = 0x70;
        inline constexpr std::uintptr_t Parent = 0x68;
    }
    namespace LRUNode {
        inline constexpr std::uintptr_t AssetID = 0x10;
        inline constexpr std::uintptr_t CachedItem = 0x38;
        inline constexpr std::uintptr_t Next = 0x0;
    }
    namespace MemEnforcedLRUCache {
        inline constexpr std::uintptr_t Head = 0x8;
    }
    namespace MeshContentProvider {
        inline constexpr std::uintptr_t Cache = 0xc8;
        inline constexpr std::uintptr_t LRUCache = 0x20;
        inline constexpr std::uintptr_t LRUHolder = 0xc8;
        inline constexpr std::uintptr_t LruHolder = 0xc8;
        inline constexpr std::uintptr_t MeshData = 0x28;
        inline constexpr std::uintptr_t ToMeshData = 0x38;
    }
    namespace MeshData {
        inline constexpr std::uintptr_t FaceEnd = 0x38;
        inline constexpr std::uintptr_t FaceStart = 0x30;
        inline constexpr std::uintptr_t VertexEnd = 0x8;
        inline constexpr std::uintptr_t VertexStart = 0x0;
    }
    namespace MeshPart {
        inline constexpr std::uintptr_t MeshId = 0x300;
        inline constexpr std::uintptr_t Texture = 0x330;
    }
    namespace Misc {
        inline constexpr std::uintptr_t StringLength = 0x10;
    }
    namespace NativeOcclusion {
        inline constexpr std::uintptr_t FlagRva = 0x8693d50;
        inline constexpr std::uintptr_t NameLengthOffset = 0x10;
        inline constexpr std::uintptr_t NameOffset = 0x8;
    }
    namespace Player {
        inline constexpr std::uintptr_t LocalPlayer = 0x120;
        inline constexpr std::uintptr_t ModelInstance = 0x288;
        inline constexpr std::uintptr_t Team = 0x2c8;
        inline constexpr std::uintptr_t UserId = 0xc0;
    }
    namespace Primitive {
        inline constexpr std::uintptr_t Flags = 0x1b6;
        inline constexpr std::uintptr_t Part = 0x210;
        inline constexpr std::uintptr_t Position = 0xd4;
        inline constexpr std::uintptr_t Rotation = 0xb0;
        inline constexpr std::uintptr_t Size = 0x1bc;
        inline constexpr std::uintptr_t Validate = 0x6;
    }
    namespace PrimitiveFlags {
        inline constexpr std::uintptr_t CanCollide = 0x8;
    }
    namespace Sound {
        inline constexpr std::uintptr_t PlaybackSpeed = 0x10c;
        inline constexpr std::uintptr_t SoundId = 0xb8;
        inline constexpr std::uintptr_t Volume = 0x120;
    }
    namespace SpecialMesh {
        inline constexpr std::uintptr_t MeshId = 0xe8;
        inline constexpr std::uintptr_t Offset = 0xa8;
        inline constexpr std::uintptr_t Scale = 0xb4;
    }
    namespace TaskScheduler {
        inline constexpr std::uintptr_t MaxFPS = 0xb0;
        inline constexpr std::uintptr_t Pointer = 0x8b79128;
    }
    namespace VisualEngine {
        inline constexpr std::uintptr_t Dimensions = 0xb10;
        inline constexpr std::uintptr_t FakeDataModel = 0xaf0;
        inline constexpr std::uintptr_t Pointer = 0x8656e40;
        inline constexpr std::uintptr_t RenderView = 0xc30;
        inline constexpr std::uintptr_t ViewMatrix = 0x1b0;
    }
    namespace Workspace {
        inline constexpr std::uintptr_t CurrentCamera = 0x4a8;
    }
    namespace WorldRoot {
        inline constexpr std::uintptr_t RaycastBoundDesc = 0x8364bb0;
        inline constexpr std::uintptr_t RaycastBoundFn = 0x90;
    }
}
