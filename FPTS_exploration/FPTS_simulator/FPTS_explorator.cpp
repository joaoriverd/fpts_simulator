// Control_tasks_tool.cpp 
//
#include "stdafx.h"
#include <limits>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>    // std::sort
#include <math.h>       // ceil
#include <chrono>
#include <random>
using namespace std;

#define PERIOD_GRAIN 5
#define LOWER_BOUND_PERIOD 10
#define UPPER_BOUND_PERIOD 80
#define LOWER_BOUND_UTILIZATION 100
#define UPPER_BOUND_UTILIZATION 600
#define TARGET_TASK        2
#define NUMBER_OF_TASKS    3
#define TRH_TARGET_TASK    NUMBER_OF_TASKS - 1
#define UTIL_GRAIN 1000
#define MAX_UTIL 980

typedef size_t task_key_type;
typedef size_t priority_type;

struct task_type
{
  size_t key;
  size_t period;
  size_t computation;
  size_t prio;
  size_t threashold;
  size_t phase;
  size_t wcrt;
  size_t bcrt;
};

struct job_type
{
  size_t key;
  task_key_type parent_task;
  size_t current_prio;
  size_t remaining_time;
  size_t restart_time;
  size_t activation_time;
};

typedef job_type running_job_type;

bool is_idle = true;

bool job_compare(const job_type& j1, const job_type& j2) {

  if (j1.parent_task == j2.parent_task)
  {
    return j1.key > j2.key;
  }

  return j1.current_prio < j2.current_prio;

}

bool schedule_next_job(std::vector<job_type>& active_jobs, std::vector<job_type>& preempted_jobs, running_job_type& running_job)
{
  std::vector<job_type>* vector_ptr = NULL;
  std::sort(active_jobs.begin(), active_jobs.end(), job_compare);
  std::sort(preempted_jobs.begin(), preempted_jobs.end(), job_compare);

  job_type job_temp = { 0,0,0,0,0,0 };

  if (active_jobs.size() > 0)
  {
    job_temp = active_jobs.back();
    vector_ptr = &active_jobs;
  }

  if (preempted_jobs.size() > 0)
  {
    if (preempted_jobs.back().current_prio >= job_temp.current_prio)
    {
      job_temp = preempted_jobs.back();
      vector_ptr = &preempted_jobs;
    }
  }

  if (NULL == vector_ptr)
  {
    return false;
  }

  if (is_idle == true)
  {
    vector_ptr->pop_back();
    running_job = job_temp;

    return true;
  }

  if (job_temp.current_prio > running_job.current_prio)
  {
    // We have to preempt the running task
    vector_ptr->pop_back();
    preempted_jobs.push_back(running_job);
    running_job = job_temp;

    return true;
  }

  return false;
}

size_t lcm(size_t a, size_t b)
{
  size_t m, n;

  m = a;
  n = b;

  while (m != n)
  {
    if (m < n)
    {
      m = m + a;
    }
    else
    {
      n = n + b;
    }
  }

  return m;
}

void fpts_simulator(std::vector<task_type>& ts)
{
  static std::map<size_t, std::vector<job_type>> released_jobs;
  static std::vector<job_type> active_jobs;
  static std::vector<job_type> preempted_jobs;
  running_job_type running_job;
  size_t simulation_time;

  // We want to simulate during 4 lcm of the periods. Hence, first find the lcm.
  size_t hyper_period = 1;
  for (const task_type& t : ts)
  {
    hyper_period = lcm(hyper_period, t.period);
  }
  simulation_time = 4 * hyper_period + hyper_period / 2;


  // First register all the tasks and plot the release time
  for (task_type& t : ts)
  {
    size_t n = simulation_time / t.period;

    size_t activation_time = t.phase;
    // Iterate over all jobs of task t and plot activation time
    for (size_t k = 0; k < n; k++)
    {
      // register two subjobs for each task instance
      job_type job = { k, t.key, t.prio, t.computation, 0, activation_time };

      released_jobs[activation_time].push_back(job);
      activation_time += t.period;
    }
  }

  // Schedule all the jobs
  running_job = { 0,0,0,0,0,0 };
  size_t current_time = 0;
  size_t prev_time = 0;
  size_t sched_time = 0;
  for (std::pair<size_t, std::vector<job_type>> new_jobs : released_jobs)
  {
    current_time = new_jobs.first;
    sched_time = current_time - prev_time;

    // First complete the schedule in the time period [prev_time, current_time)
    while (sched_time > 0)
    {
      if (running_job.remaining_time <= sched_time && false == is_idle)
      {
        // The running job ends, schedule next job.
        size_t finalization_time = running_job.restart_time + running_job.remaining_time;
        size_t response_time = finalization_time - running_job.activation_time;

        if (current_time > 2 * hyper_period && current_time <= 4 * hyper_period)
        {
          if (response_time > ts[running_job.parent_task].wcrt)
          {
            ts[running_job.parent_task].wcrt = response_time;
          }

          if (response_time < ts[running_job.parent_task].bcrt)
          {
            ts[running_job.parent_task].bcrt = response_time;
          }
        }

        sched_time -= running_job.remaining_time;
        is_idle = true;

        if (sched_time > 0)
        {
          if ( schedule_next_job(active_jobs, preempted_jobs, running_job) )
          {
            running_job.restart_time = current_time - sched_time;

            // lift priority to threashold
            running_job.current_prio = ts[running_job.parent_task].threashold;

            is_idle = false;
          }
        }

        //grasp_file << "\n";
      }
      else
      {
        // The running job has not ended yet.
        running_job.remaining_time -= sched_time;
        running_job.restart_time += sched_time;
        sched_time = 0;
      }
    }

    // Add the released jobs to active jobs and order by priorities
    active_jobs.insert(active_jobs.end(), new_jobs.second.begin(), new_jobs.second.end());

    // Decide which job to schedule between the running job, the active jobs and the preemented jobs.
    bool was_idle = is_idle;
    running_job_type prev_job = running_job;
    if ( schedule_next_job(active_jobs, preempted_jobs, running_job) )
    {
      running_job.restart_time = current_time;

      // lift priority to threashold
      running_job.current_prio = ts[running_job.parent_task].threashold;

      is_idle = false;
    }

    prev_time = current_time;
  }

  // Clean all vectors used to prepare for next iteration
  released_jobs.clear();
  active_jobs.clear();
  preempted_jobs.clear();
}

size_t ceil_div(size_t n, size_t d)
{
  size_t r = n / d;

  if (n%d > 0)
  {
    r++;
  }

  return r;
}

size_t worst_case_hold_time(std::vector<task_type>& ts, size_t t_key)
{
  size_t wcht = ts[t_key].computation;
  size_t wcht_old = 0;
  size_t sum = 0;

  while (wcht != wcht_old)
  {
    wcht_old = wcht;
    sum = 0;
    for (const task_type& t : ts)
    {
      if (t.prio > ts[t_key].threashold)
      {
        sum += ceil_div(wcht, t.period)*t.computation;
      }
    }
    
    wcht = ts[t_key].computation + sum;
  }

  return wcht;
}

size_t best_case_hold_time(std::vector<task_type>& ts, size_t t_key)
{
  size_t bcht = worst_case_hold_time(ts,t_key);
  size_t bcht_old = std::numeric_limits<size_t>::max();
  size_t sum = 0;
  size_t temp;

  while (bcht != bcht_old)
  {
    bcht_old = bcht;
    sum = 0;
    for (const task_type& t : ts)
    {
      if (t.prio > ts[t_key].threashold)
      {
        temp = ceil_div(bcht, t.period) - 1;
        if (temp > 0)
        {
          sum += temp*t.computation;
        }
      }
    }

    bcht = ts[t_key].computation + sum;
  }

  return bcht;
}

void print_taskset(std::vector<task_type>& ts)
{
  size_t lcm_periods = 1;
  for (const task_type& t : ts)
  {
    lcm_periods = lcm(lcm_periods, t.period);
  }

  std::cout << "Non-trivial taskset found for the last task. lcm = " << lcm_periods << "\n";

  // Print charachteristics of task set
  std::cout << "Task | Period | Exec. | Prio | Thrs | Phase | BCRT | WCRT\n";
  std::cout << "------------------------------------------------\n";
  for (const task_type& t : ts)
  {
    std::cout << " " << t.key << "   |   " << t.period << "   |  " << t.computation << "   |  " << t.prio << "   |  ";
    std::cout << t.threashold << "   |  " << t.phase << "   |  " << t.bcrt << "   |  " << t.wcrt << "\n";
  }
  std::cout << "------------------------------------------------\n\n";
}

size_t get_random_number(size_t lower_bound, size_t upper_bound)
{
  static unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
  static std::default_random_engine e(seed1);
  std::uniform_int_distribution <size_t> d(lower_bound, upper_bound);
  return d(e);
}

void generate_random_taskset(std::vector<task_type>& ts)
{
  task_type t1 = { 0,0,0,0,0,0,0,std::numeric_limits<size_t>::max() };
  double upper_bound_util;
  double cummulative_utilization;
  bool valid_task_set = false;

  // Initialise key, priority and threshold for all the tasks
  for (size_t i = 0; i < NUMBER_OF_TASKS ; i++)
  {
    t1.key = i;
    t1.prio = NUMBER_OF_TASKS - i;
    if (i == TARGET_TASK)
    {
      t1.threashold = TRH_TARGET_TASK;
    }
    else
    {
      t1.threashold = t1.prio;
    }

    ts.push_back(t1);
  }

  // Initialise period and execution time for all the tasks
  while (!valid_task_set)
  {
    cummulative_utilization = 0;
    for (task_type& t : ts)
    {
      t.period = get_random_number(LOWER_BOUND_PERIOD, UPPER_BOUND_PERIOD);
      t.period -= t.period % PERIOD_GRAIN;
      if (t.key != ts.back().key)
      {
        upper_bound_util = min((double)UPPER_BOUND_UTILIZATION, MAX_UTIL - cummulative_utilization);
        t.computation = get_random_number(t.period*LOWER_BOUND_UTILIZATION / UTIL_GRAIN, t.period*upper_bound_util / UTIL_GRAIN);
      }
      else
      {
        // If it is the last task, assign as much as utilization possible
        t.computation = t.period * (MAX_UTIL - cummulative_utilization) / UTIL_GRAIN;
      }

      cummulative_utilization += t.computation * UTIL_GRAIN / t.period;
    }

    if (ts.back().computation > 0)
    {
      valid_task_set = true;
    }

  }
}

int main(int argc, char** argv) {
  std::vector<task_type> task_set;

  // Find a non trivial example where the bcrt of the last task is not equal to its bcht
  while (true)
  {
    generate_random_taskset(task_set);
    
    // Calculate the trivial best case hold time of the last task
    size_t bcht = best_case_hold_time(task_set, TARGET_TASK);

    // Force phases to hypothetical optimal instant of task to analyse
    for (task_type& t : task_set)
    {
      // Phases for preemptive tasks t_h
      if (t.prio > task_set[TARGET_TASK].threashold)
      {
        t.phase = (bcht % t.period);
      }
      else if (t.prio > task_set[TARGET_TASK].prio && task_set[TARGET_TASK].threashold >= t.prio)
      {
        // Phases for delaying tasks t_d
        t.phase = 1;
      }
      else if (task_set[TARGET_TASK].prio >= t.prio && t.threashold >= task_set[TARGET_TASK].prio)
      {
        // Phases for blocking tasks t_b
        t.phase = 0;
      }
      else
      {
        t.phase = 0;
      }
    }

    // Run simulator
    fpts_simulator(task_set);

    if (task_set[TARGET_TASK].bcrt != bcht)
    {
      // Non trival task set found
      print_taskset(task_set);
      //break;
    }

    // Clear task set to prepare for next iteration
    task_set.clear();
  }

  return 0;
}