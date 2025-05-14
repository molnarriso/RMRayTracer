#define WIN32_LEAN_AND_MEAN
#define M_PI 3.14159265358979323846
#include <windows.h>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <string>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include <format>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#include "Vec3.h"
#include "Ray.h"
#include "Camera.h"
#include "Sphere.h"
#include "ColorUtils.h"
#include "Material.h"
#include "SceneLoader.h"

const int IMAGE_WIDTH = 1920;
const int IMAGE_HEIGHT = 1080;
const int NUM_THREADS = 16;
const double REFLECTION_EPSILON = 1e-4;

HWND g_hwnd = nullptr;
std::vector<uint32_t> g_pixelBuffer(IMAGE_WIDTH* IMAGE_HEIGHT);
BITMAPINFO g_bitmapInfo = { 0 };

struct FrameInfo {
    long long frameNumber;
};
FrameInfo g_currentFrameInfo;

std::vector<std::jthread> g_threads;
std::mutex g_renderMutex;
std::condition_variable g_workerStartCv;
std::condition_variable g_mainWaitCv;
std::atomic<long long> g_targetFrameId = -1;
std::atomic<int> g_workersDoneCount = 0;
std::atomic<bool> g_shutdownThreads = false;

Scene g_current_scene;
Camera g_camera;

Vec3 trace_ray(const Ray& ray, int depth, std::mt19937& rng); // Forward declaration

struct ThreadRenderTask {
    int threadId;
    int startY;
    int endY;
    std::vector<uint32_t>* pixelBuffer_ptr;
};
std::vector<ThreadRenderTask> g_renderTasks;

void render_chunk(const FrameInfo& frame_info, const ThreadRenderTask& task, std::mt19937& rng_for_thread) {
    for (int y = task.startY; y < task.endY; ++y) {
        for (int x = 0; x < IMAGE_WIDTH; ++x) {
            Vec3 accumulated_color(0.0, 0.0, 0.0);
            int samples = g_current_scene.samples_per_pixel > 0 ? g_current_scene.samples_per_pixel : 1;

            for (int s = 0; s < samples; ++s) {
                std::uniform_real_distribution<double> jitter_dist(0.0, 1.0);
                double dx = (samples > 1) ? jitter_dist(rng_for_thread) : 0.5;
                double dy = (samples > 1) ? jitter_dist(rng_for_thread) : 0.5;
                double u = (static_cast<double>(x) + dx) / IMAGE_WIDTH;
                double v = (static_cast<double>(y) + dy) / IMAGE_HEIGHT;
                Ray primary_ray = g_camera.get_ray(u, v);
                accumulated_color = accumulated_color + trace_ray(primary_ray, 0, rng_for_thread);
            }
            Vec3 final_pixel_color = accumulated_color / static_cast<double>(samples);
            (*task.pixelBuffer_ptr)[y * IMAGE_WIDTH + x] = vec3_to_uint32_color(final_pixel_color);
        }
    }
}

Vec3 trace_ray(const Ray& ray, int depth, std::mt19937& rng) {
    if (depth >= g_current_scene.max_ray_depth) {
        return Vec3(0.0, 0.0, 0.0);
    }

    double closest_t = -1.0;
    const Sphere* hit_sphere = nullptr;
    Vec3 hit_point;
    Vec3 surface_normal;

    for (const auto& sphere_obj : g_current_scene.objects) {
        double t_intersection;
        Vec3 current_hit_point;
        Vec3 current_normal;
        if (sphere_obj.intersect(ray, t_intersection, current_hit_point, current_normal)) {
            double t_min_check = (depth == 0) ? 1e-4 : REFLECTION_EPSILON;
            if (t_intersection > t_min_check && (closest_t < 0 || t_intersection < closest_t)) {
                closest_t = t_intersection;
                hit_sphere = &sphere_obj;
                hit_point = current_hit_point;
                surface_normal = current_normal;
            }
        }
    }

    if (!hit_sphere) {
        return g_current_scene.background_color;
    }

    const Material& material = hit_sphere->material;
    Vec3 final_color = material.emission_color;
    double base_color_contribution_factor = 1.0 - material.reflectivity;

    if (base_color_contribution_factor > 1e-5) {
        final_color = final_color + material.base_color * base_color_contribution_factor;
    }

    if (material.reflectivity > 1e-5) {
        Vec3 incident_dir = ray.direction;
        Vec3 perfect_reflection_dir = incident_dir - surface_normal * (2.0 * Vec3::dot(incident_dir, surface_normal));
        perfect_reflection_dir = perfect_reflection_dir.normalize();
        Vec3 scattered_reflection_dir;

        if (material.roughness < 1e-5) {
            scattered_reflection_dir = perfect_reflection_dir;
        }
        else {
            Vec3 tangent, bitangent;
            create_onb(surface_normal, tangent, bitangent);
            Vec3 sample_local_hemisphere = random_cosine_direction(rng);
            Vec3 random_world_dir = (tangent * sample_local_hemisphere.x +
                bitangent * sample_local_hemisphere.y +
                surface_normal * sample_local_hemisphere.z).normalize();
            scattered_reflection_dir = (perfect_reflection_dir * (1.0 - material.roughness) +
                random_world_dir * material.roughness).normalize();
        }
        Ray reflection_ray(hit_point + surface_normal * REFLECTION_EPSILON, scattered_reflection_dir);
        Vec3 reflected_light = trace_ray(reflection_ray, depth + 1, rng);
        final_color = final_color + reflected_light * material.reflectivity;
    }
    return final_color;
}

void render_chunk_loop(const ThreadRenderTask& task) {
    long long worker_last_completed_frame_id = -1;
    std::mt19937 thread_rng(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + task.threadId);
    while (true) {
        FrameInfo local_frame_info;
        long long current_frame_to_render;
        {
            std::unique_lock<std::mutex> lock(g_renderMutex);
            g_workerStartCv.wait(lock, [&] {
                return g_targetFrameId.load(std::memory_order_acquire) > worker_last_completed_frame_id ||
                    g_shutdownThreads.load(std::memory_order_acquire);
                });
            if (g_shutdownThreads.load(std::memory_order_acquire)) {
                break;
            }
            current_frame_to_render = g_targetFrameId.load(std::memory_order_acquire);
            local_frame_info = g_currentFrameInfo;
        }
        render_chunk(local_frame_info, task, thread_rng);
        worker_last_completed_frame_id = current_frame_to_render;
        if (g_workersDoneCount.fetch_add(1, std::memory_order_acq_rel) + 1 == NUM_THREADS) {
            std::lock_guard<std::mutex> lock(g_renderMutex);
            g_mainWaitCv.notify_one();
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!g_pixelBuffer.empty()) {
            StretchDIBits(hdc, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT,
                g_pixelBuffer.data(), &g_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void setup_camera_defaults() {
    g_camera.position = Vec3(0, 1.0, 4.0);
    g_camera.look_at_target = Vec3(0, 0.5, 0);
    g_camera.world_up_vector = Vec3(0, 1, 0);
    g_camera.fov_degrees = 60.0;
    g_camera.initialize(static_cast<double>(IMAGE_WIDTH) / IMAGE_HEIGHT);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bitmapInfo.bmiHeader.biWidth = IMAGE_WIDTH;
    g_bitmapInfo.bmiHeader.biHeight = -IMAGE_HEIGHT; // Top-down DIB
    g_bitmapInfo.bmiHeader.biPlanes = 1;
    g_bitmapInfo.bmiHeader.biBitCount = 32;
    g_bitmapInfo.bmiHeader.biCompression = BI_RGB;

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"RayTracerWindowClass";
    RegisterClassEx(&wc);

    RECT window_rect = { 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT };
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowEx(0, L"RayTracerWindowClass", L"CPU Ray Tracer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_hwnd) {
        MessageBox(nullptr, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    g_renderTasks.resize(NUM_THREADS);
    int rowsPerThread = IMAGE_HEIGHT / NUM_THREADS;
    int current_y_offset = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        g_renderTasks[i].threadId = i;
        g_renderTasks[i].startY = current_y_offset;
        g_renderTasks[i].endY = current_y_offset + rowsPerThread;
        g_renderTasks[i].pixelBuffer_ptr = &g_pixelBuffer;
        if (i == NUM_THREADS - 1) g_renderTasks[i].endY = IMAGE_HEIGHT;
        current_y_offset = g_renderTasks[i].endY;
    }

    g_threads.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i) {
        g_threads.emplace_back(render_chunk_loop, std::cref(g_renderTasks[i]));
    }

    const double CAMERA_MOVE_STEP = 0.1;
    const double CAMERA_ROTATE_STEP = 0.03;
    bool quit_flag = false;
    long long frame_counter = 0;

    setup_camera_defaults();

    while (!quit_flag) {
        MSG msg = { 0 };
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) quit_flag = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (quit_flag) break;

        g_current_scene = load_scene_from_file("scene.txt"); // Scene reloaded each frame
        if (g_current_scene.objects.empty() && g_current_scene.max_ray_depth <= 0) {
            OutputDebugStringA("Warning: Scene may be empty or invalid after loading.\n");
        }

        bool camera_has_moved = false;
        if (GetActiveWindow() == g_hwnd) {
            if (GetAsyncKeyState('W') & 0x8000) { g_camera.move_forward(CAMERA_MOVE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState('S') & 0x8000) { g_camera.move_forward(-CAMERA_MOVE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState('A') & 0x8000) { g_camera.move_sideways(-CAMERA_MOVE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState('D') & 0x8000) { g_camera.move_sideways(CAMERA_MOVE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) { g_camera.move_vertical(CAMERA_MOVE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState(VK_LCONTROL) & 0x8000 || GetAsyncKeyState(VK_RCONTROL) & 0x8000) { g_camera.move_vertical(-CAMERA_MOVE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) { g_camera.rotate_yaw(-CAMERA_ROTATE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { g_camera.rotate_yaw(CAMERA_ROTATE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState(VK_UP) & 0x8000) { g_camera.rotate_pitch(CAMERA_ROTATE_STEP); camera_has_moved = true; }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) { g_camera.rotate_pitch(-CAMERA_ROTATE_STEP); camera_has_moved = true; }
            if (camera_has_moved) g_camera.update_orientation_vectors();
        }

        g_currentFrameInfo.frameNumber = frame_counter;
        g_workersDoneCount.store(0, std::memory_order_relaxed);
        g_targetFrameId.store(frame_counter, std::memory_order_release);
        g_workerStartCv.notify_all();
        {
            std::unique_lock<std::mutex> lock(g_renderMutex);
            g_mainWaitCv.wait(lock, [&] {
                return g_workersDoneCount.load(std::memory_order_acquire) == NUM_THREADS;
                });
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
        frame_counter++;
        if (frame_counter % 100 == 0) {
            OutputDebugStringA(std::format("Frame: {}\n", frame_counter).c_str());
        }
    }

    g_shutdownThreads.store(true, std::memory_order_release);
    g_workerStartCv.notify_all();
    g_threads.clear(); // jthreads join on destruction

    return 0;
}