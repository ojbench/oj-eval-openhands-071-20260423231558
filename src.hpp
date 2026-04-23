#pragma once
#include <random>
#include <algorithm>
#include <queue>
#include <cmath>
#include <map>

namespace oj {

auto generate_tasks(const Description &desc) -> std::vector <Task> {
    std::vector<Task> tasks;
    tasks.reserve(desc.task_count);
    
    std::mt19937_64 rng(42);
    
    // Calculate target sums
    time_t target_exec_sum = (desc.execution_time_sum.min + desc.execution_time_sum.max) / 2;
    priority_t target_priority_sum = (desc.priority_sum.min + desc.priority_sum.max) / 2;
    
    time_t current_exec_sum = 0;
    priority_t current_priority_sum = 0;
    
    for (task_id_t i = 0; i < desc.task_count; ++i) {
        Task task;
        
        // Launch time - spread tasks over time
        task.launch_time = std::uniform_int_distribution<time_t>(0, desc.deadline_time.max / 2)(rng);
        
        // Execution time
        time_t remaining_tasks = desc.task_count - i;
        time_t max_exec = std::min(
            desc.execution_time_single.max,
            desc.execution_time_sum.max - current_exec_sum - (remaining_tasks - 1) * desc.execution_time_single.min
        );
        time_t min_exec = std::max(
            desc.execution_time_single.min,
            desc.execution_time_sum.min - current_exec_sum - (remaining_tasks - 1) * desc.execution_time_single.max
        );
        
        if (min_exec > max_exec) min_exec = max_exec;
        
        task.execution_time = std::uniform_int_distribution<time_t>(min_exec, max_exec)(rng);
        current_exec_sum += task.execution_time;
        
        // Priority
        priority_t max_priority = std::min(
            desc.priority_single.max,
            desc.priority_sum.max - current_priority_sum - (remaining_tasks - 1) * desc.priority_single.min
        );
        priority_t min_priority = std::max(
            desc.priority_single.min,
            desc.priority_sum.min - current_priority_sum - (remaining_tasks - 1) * desc.priority_single.max
        );
        
        if (min_priority > max_priority) min_priority = max_priority;
        
        task.priority = std::uniform_int_distribution<priority_t>(min_priority, max_priority)(rng);
        current_priority_sum += task.priority;
        
        // Deadline - ensure task is feasible
        time_t min_time_needed = task.launch_time + PublicInformation::kStartUp + PublicInformation::kSaving +
            (time_t)std::ceil(task.execution_time / std::pow(PublicInformation::kCPUCount, PublicInformation::kAccel));
        
        time_t min_deadline = std::max(min_time_needed + 1, desc.deadline_time.min);
        time_t max_deadline = desc.deadline_time.max;
        
        if (min_deadline > max_deadline) min_deadline = max_deadline;
        
        task.deadline = std::uniform_int_distribution<time_t>(min_deadline, max_deadline)(rng);
        
        tasks.push_back(task);
    }
    
    std::sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.launch_time < b.launch_time;
    });
    
    return tasks;
}

} // namespace oj

namespace oj {

enum class TaskState {
    Free,
    Launching,
    Executing,
    Saving
};

struct TaskInfo {
    task_id_t id;
    time_t execution_time;
    time_t deadline;
    priority_t priority;
    double work_done;
    TaskState state;
    cpu_id_t current_cpu;
    time_t state_start_time;
    
    TaskInfo() : id(0), execution_time(0), deadline(0), priority(0), 
                 work_done(0), state(TaskState::Free), current_cpu(0), state_start_time(0) {}
};

auto schedule_tasks(time_t time, std::vector <Task> list, const Description &desc) -> std::vector<Policy> {
    static task_id_t task_id = 0;
    static std::map<task_id_t, TaskInfo> all_tasks;
    
    const task_id_t first_id = task_id;
    const task_id_t last_id = task_id + list.size();
    
    // Add new tasks
    for (size_t i = 0; i < list.size(); ++i) {
        TaskInfo info;
        info.id = first_id + i;
        info.execution_time = list[i].execution_time;
        info.deadline = list[i].deadline;
        info.priority = list[i].priority;
        info.work_done = 0;
        info.state = TaskState::Free;
        all_tasks[info.id] = info;
    }
    
    task_id = last_id;
    
    std::vector<Policy> policies;
    
    // Update task states based on time
    cpu_id_t cpu_used = 0;
    
    for (auto& [tid, tinfo] : all_tasks) {
        if (tinfo.state == TaskState::Launching) {
            time_t elapsed = time - tinfo.state_start_time;
            if (elapsed >= PublicInformation::kStartUp) {
                tinfo.state = TaskState::Executing;
            }
            cpu_used += tinfo.current_cpu;
        } else if (tinfo.state == TaskState::Executing) {
            cpu_used += tinfo.current_cpu;
        } else if (tinfo.state == TaskState::Saving) {
            time_t elapsed = time - tinfo.state_start_time;
            if (elapsed >= PublicInformation::kSaving) {
                // Task is now free
                tinfo.state = TaskState::Free;
                tinfo.current_cpu = 0;
            } else {
                cpu_used += tinfo.current_cpu;
            }
        }
    }
    
    // Decide which executing tasks to save
    for (auto& [tid, tinfo] : all_tasks) {
        if (tinfo.state == TaskState::Executing) {
            time_t elapsed = time - tinfo.state_start_time;
            double work = time_policy(elapsed, tinfo.current_cpu);
            
            // Save if task is complete or deadline is approaching
            if (work >= tinfo.execution_time - tinfo.work_done || 
                time + PublicInformation::kSaving >= tinfo.deadline) {
                policies.push_back(Saving{tid});
                tinfo.work_done += work;
                tinfo.state = TaskState::Saving;
                tinfo.state_start_time = time;
            }
        }
    }
    
    // Update cpu_used after saving
    cpu_used = 0;
    for (auto& [tid, tinfo] : all_tasks) {
        if (tinfo.state == TaskState::Launching || 
            tinfo.state == TaskState::Executing || 
            tinfo.state == TaskState::Saving) {
            cpu_used += tinfo.current_cpu;
        }
    }
    
    // Select tasks to launch
    std::vector<task_id_t> pending_tasks;
    for (auto& [tid, tinfo] : all_tasks) {
        if (tinfo.state == TaskState::Free && 
            tinfo.work_done < tinfo.execution_time && 
            time < tinfo.deadline) {
            pending_tasks.push_back(tid);
        }
    }
    
    // Sort by deadline first, then priority
    std::sort(pending_tasks.begin(), pending_tasks.end(), [&](task_id_t a, task_id_t b) {
        auto& ta = all_tasks[a];
        auto& tb = all_tasks[b];
        
        if (ta.deadline != tb.deadline) return ta.deadline < tb.deadline;
        return ta.priority > tb.priority;
    });
    
    // Launch tasks
    for (auto tid : pending_tasks) {
        if (cpu_used >= PublicInformation::kCPUCount) break;
        
        auto& tinfo = all_tasks[tid];
        time_t time_available = tinfo.deadline - time - PublicInformation::kSaving - PublicInformation::kStartUp;
        
        if (time_available <= 0) continue;
        
        // Allocate CPUs
        cpu_id_t cpus_available = PublicInformation::kCPUCount - cpu_used;
        if (cpus_available == 0) break;
        
        // Use a reasonable number of CPUs (not too many)
        cpu_id_t cpus_to_use = std::min(cpus_available, (cpu_id_t)20);
        
        policies.push_back(Launch{cpus_to_use, tid});
        tinfo.state = TaskState::Launching;
        tinfo.current_cpu = cpus_to_use;
        tinfo.state_start_time = time;
        cpu_used += cpus_to_use;
    }
    
    return policies;
}

} // namespace oj
