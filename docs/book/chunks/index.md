# Game Engine Architecture 3e - Reading Chunks (Smart Overlap)

Source: `C:\dev\mark\marble\docs\book\game-engine-architecture-3.pdf`
Total pages: 1240

Rules: merge to 8-28 pages; add overlap only on risky boundaries.

| Chunk | Content | Base Pages | Output Pages | Overlap Added | File |
|---|---|---:|---:|---|---|
| 001 | Front Matter | 1-21 | 1-22 | next | `chunk-001-Front-Matter.pdf` |
| 002 | 1 Introduction -> 1.4 Engine Differences across Genres | 22-49 | 21-50 | prev,next | `chunk-002-1-Introduction.pdf` |
| 003 | 1.5 Game Engine Survey -> 1.6 Runtime Engine Architecture | 50-77 | 49-78 | prev,next | `chunk-003-1.5-Game-Engine-Survey.pdf` |
| 004 | 1.7 Tools and the Asset Pipeline | 78-87 | 77-87 | prev | `chunk-004-1.7-Tools-and-the-Asset-Pipeline.pdf` |
| 005 | 2 Tools of the Trade | 88-96 | 88-96 | none | `chunk-005-2-Tools-of-the-Trade.pdf` |
| 006 | 2.2 Compilers, Linkers and IDEs -> 2.5 Other Tools | 97-123 | 97-124 | next | `chunk-006-2.2-Compilers,-Linkers-and-IDEs.pdf` |
| 007 | 3 Fundamentals of Software Engineering for Games -> 3.2 Catching and Handling Errors | 124-149 | 123-150 | prev,next | `chunk-007-3-Fundamentals-of-Software-Engineering-for-Games.pdf` |
| 008 | 3.3 Data, Code and Memory Layout | 150-182 | 149-182 | prev | `chunk-008-3.3-Data,-Code-and-Memory-Layout.pdf` |
| 009 | 3.4 Computer Hardware Fundamentals | 183-199 | 183-199 | none | `chunk-009-3.4-Computer-Hardware-Fundamentals.pdf` |
| 010 | 3.5 Memory Architectures | 200-221 | 200-222 | next | `chunk-010-3.5-Memory-Architectures.pdf` |
| 011 | 4 Parallelism and Concurrent Programming -> 4.3 Explicit Parallelism | 222-248 | 221-248 | prev | `chunk-011-4-Parallelism-and-Concurrent-Programming.pdf` |
| 012 | 4.4 Operating System Fundamentals | 249-274 | 249-274 | none | `chunk-012-4.4-Operating-System-Fundamentals.pdf` |
| 013 | 4.5 Introduction to Concurrent Programming -> 4.6 Thread Synchronization Primitives | 275-299 | 275-300 | next | `chunk-013-4.5-Introduction-to-Concurrent-Programming.pdf` |
| 014 | 4.7 Problems with Lock-Based Concurrency -> 4.8 Some Rules of Thumb for Concurrency | 300-307 | 299-308 | prev,next | `chunk-014-4.7-Problems-with-Lock-Based-Concurrency.pdf` |
| 015 | 4.9 Lock-Free Concurrency | 308-349 | 307-349 | prev | `chunk-015-4.9-Lock-Free-Concurrency.pdf` |
| 016 | 4.10 SIMD/Vector Processing -> 4.11 Introduction to GPGPU Programming | 350-377 | 350-378 | next | `chunk-016-4.10-SIMDVector-Processing.pdf` |
| 017 | 5 3D Math for Games -> 5.2 Points and Vectors | 378-393 | 377-394 | prev,next | `chunk-017-5-3D-Math-for-Games.pdf` |
| 018 | 5.3 Matrices -> 5.4 Quaternions | 394-421 | 393-421 | prev | `chunk-018-5.3-Matrices.pdf` |
| 019 | 5.5 Comparison of Rotational Representations -> 5.7 Random Number Generation | 422-435 | 422-436 | next | `chunk-019-5.5-Comparison-of-Rotational-Representations.pdf` |
| 020 | 6 Engine Support Systems -> 6.2 Memory Management | 436-459 | 435-460 | prev,next | `chunk-020-6-Engine-Support-Systems.pdf` |
| 021 | 6.3 Containers | 460-474 | 459-474 | prev | `chunk-021-6.3-Containers.pdf` |
| 022 | 6.4 Strings -> 6.5 Engine Configuration | 475-499 | 475-500 | next | `chunk-022-6.4-Strings.pdf` |
| 023 | 7 Resources and the File System -> 7.1 File System | 500-511 | 499-511 | prev | `chunk-023-7-Resources-and-the-File-System.pdf` |
| 024 | 7.2 The Resource Manager | 512-543 | 512-544 | next | `chunk-024-7.2-The-Resource-Manager.pdf` |
| 025 | 8 The Game Loop and Real-Time Simulation -> 8.5 Measuring and Dealing with Time | 544-562 | 543-562 | prev | `chunk-025-8-The-Game-Loop-and-Real-Time-Simulation.pdf` |
| 026 | 8.6 Multiprocessor Game Loops | 563-577 | 563-577 | none | `chunk-026-8.6-Multiprocessor-Game-Loops.pdf` |
| 027 | 9 Human Interface Devices -> 9.5 Game Engine HID Systems | 578-605 | 578-606 | next | `chunk-027-9-Human-Interface-Devices.pdf` |
| 028 | 9.6 Human Interface Devices in Practice -> 10.8 In-Game Profiling | 606-633 | 605-634 | prev,next | `chunk-028-9.6-Human-Interface-Devices-in-Practice.pdf` |
| 029 | 10.9 In-Game Memory Stats and Leak Detection -> 11.1 Foundations of Depth-Buffered Triangle Rasterization | 634-685 | 633-685 | prev | `chunk-029-10.9-In-Game-Memory-Stats-and-Leak-Detection.pdf` |
| 030 | 11.2 The Rendering Pipeline | 686-715 | 686-715 | none | `chunk-030-11.2-The-Rendering-Pipeline.pdf` |
| 031 | 11.3 Advanced Lighting and Global Illumination -> 11.5 Further Reading | 716-739 | 716-739 | none | `chunk-031-11.3-Advanced-Lighting-and-Global-Illumination.pdf` |
| 032 | 12 Animation Systems -> 12.3 Poses | 740-752 | 740-752 | none | `chunk-032-12-Animation-Systems.pdf` |
| 033 | 12.4 Clips -> 12.5 Skinning and Matrix Palette Generation | 753-773 | 753-773 | none | `chunk-033-12.4-Clips.pdf` |
| 034 | 12.6 Animation Blending -> 12.7 Post-Processing | 774-795 | 774-796 | next | `chunk-034-12.6-Animation-Blending.pdf` |
| 035 | 12.8 Compression Techniques -> 12.9 The Animation Pipeline | 796-804 | 795-804 | prev | `chunk-035-12.8-Compression-Techniques.pdf` |
| 036 | 12.10 Action State Machines | 805-824 | 805-824 | none | `chunk-036-12.10-Action-State-Machines.pdf` |
| 037 | 12.11 Constraints | 825-835 | 825-835 | none | `chunk-037-12.11-Constraints.pdf` |
| 038 | 13 Collision and Rigid Body Dynamics -> 13.2 Collision/Physics Middleware | 836-843 | 836-844 | next | `chunk-038-13-Collision-and-Rigid-Body-Dynamics.pdf` |
| 039 | 13.3 The Collision Detection System | 844-872 | 843-872 | prev | `chunk-039-13.3-The-Collision-Detection-System.pdf` |
| 040 | 13.4 Rigid Body Dynamics | 873-910 | 873-910 | none | `chunk-040-13.4-Rigid-Body-Dynamics.pdf` |
| 041 | 13.5 Integrating a Physics Engine into Your Game -> 13.6 Advanced Physics Features | 911-929 | 911-929 | none | `chunk-041-13.5-Integrating-a-Physics-Engine-into-Your-Game.pdf` |
| 042 | 14 Audio -> 14.1 The Physics of Sound | 930-942 | 930-942 | none | `chunk-042-14-Audio.pdf` |
| 043 | 14.2 The Mathematics of Sound | 943-959 | 943-960 | next | `chunk-043-14.2-The-Mathematics-of-Sound.pdf` |
| 044 | 14.3 The Technology of Sound | 960-973 | 959-973 | prev | `chunk-044-14.3-The-Technology-of-Sound.pdf` |
| 045 | 14.4 Rendering Audio in 3D | 974-992 | 974-992 | none | `chunk-045-14.4-Rendering-Audio-in-3D.pdf` |
| 046 | 14.5 Audio Engine Architecture | 993-1013 | 993-1013 | none | `chunk-046-14.5-Audio-Engine-Architecture.pdf` |
| 047 | 14.6 Game-Specific Audio Features | 1014-1033 | 1014-1034 | next | `chunk-047-14.6-Game-Specific-Audio-Features.pdf` |
| 048 | 15 Introduction to Gameplay Systems -> 15.4 The Game World Editor | 1034-1057 | 1033-1058 | prev,next | `chunk-048-15-Introduction-to-Gameplay-Systems.pdf` |
| 049 | 16 Runtime Gameplay Foundation Systems -> 16.2 Runtime Object Model Architectures | 1058-1080 | 1057-1080 | prev | `chunk-049-16-Runtime-Gameplay-Foundation-Systems.pdf` |
| 050 | 16.3 World Chunk Data Formats -> 16.5 Object References and World Queries | 1081-1104 | 1081-1104 | none | `chunk-050-16.3-World-Chunk-Data-Formats.pdf` |
| 051 | 16.6 Updating Game Objects in Real Time -> 16.7 Applying Concurrency to Game Object Updates | 1105-1132 | 1105-1132 | none | `chunk-051-16.6-Updating-Game-Objects-in-Real-Time.pdf` |
| 052 | 16.8 Events and Message-Passing | 1133-1152 | 1133-1152 | none | `chunk-052-16.8-Events-and-Message-Passing.pdf` |
| 053 | 16.9 Scripting -> 16.10 High-Level Game Flow | 1153-1179 | 1153-1180 | next | `chunk-053-16.9-Scripting.pdf` |
| 054 | 17 You Mean There’s More? -> 17.2 Gameplay Systems | 1180-1240 | 1179-1240 | prev | `chunk-054-17-You-Mean-There’s-More.pdf` |
