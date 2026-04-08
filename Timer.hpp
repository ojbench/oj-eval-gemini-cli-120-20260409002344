#pragma once

#include "Task.hpp"
#include <vector>

class TimingWheel;
class Timer;

class TaskNode {
    friend class TimingWheel;
    friend class Timer;
public:
    TaskNode(Task* task) : task(task), next(nullptr), prev(nullptr), time(0) {}
private:
    Task* task;
    TaskNode* next, *prev;
    size_t time;
};

class TimingWheel {
    friend class Timer;
public:
    TimingWheel(size_t size, size_t interval) : size(size), interval(interval), current_slot(0) {
        slots = new TaskNode*[size];
        for (size_t i = 0; i < size; ++i) {
            slots[i] = new TaskNode(nullptr);
            slots[i]->next = slots[i];
            slots[i]->prev = slots[i];
        }
    }
    ~TimingWheel() {
        for (size_t i = 0; i < size; ++i) {
            TaskNode* curr = slots[i]->next;
            while (curr != slots[i]) {
                TaskNode* next = curr->next;
                delete curr;
                curr = next;
            }
            delete slots[i];
        }
        delete[] slots;
    }

    void add(size_t slot, TaskNode* node) {
        TaskNode* head = slots[slot];
        node->next = head;
        node->prev = head->prev;
        head->prev->next = node;
        head->prev = node;
    }

    TaskNode* take_all(size_t slot) {
        TaskNode* head = slots[slot];
        if (head->next == head) return nullptr;
        TaskNode* first = head->next;
        TaskNode* last = head->prev;
        first->prev = nullptr;
        last->next = nullptr;
        head->next = head;
        head->prev = head;
        return first;
    }

private:
    const size_t size, interval;
    size_t current_slot;
    TaskNode** slots;
};

class Timer {
public:
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    Timer() {
        w0 = new TimingWheel(60, 1);
        w1 = new TimingWheel(60, 60);
        w2 = new TimingWheel(24, 3600);
    }

    ~Timer() {
        delete w0;
        delete w1;
        delete w2;
    }

    TaskNode* addTask(Task* task) {
        TaskNode* node = new TaskNode(task);
        node->time = task->getFirstInterval();
        schedule(node);
        return node;
    }

    void cancelTask(TaskNode *p) {
        if (p) {
            if (p->prev && p->next) {
                p->prev->next = p->next;
                p->next->prev = p->prev;
            }
            delete p;
        }
    }

    std::vector<Task*> tick() {
        w0->current_slot = (w0->current_slot + 1) % w0->size;
        if (w0->current_slot == 0) {
            w1->current_slot = (w1->current_slot + 1) % w1->size;
            if (w1->current_slot == 0) {
                w2->current_slot = (w2->current_slot + 1) % w2->size;
                TaskNode* list2 = w2->take_all(w2->current_slot);
                while (list2) {
                    TaskNode* next = list2->next;
                    list2->time %= w2->interval;
                    size_t slot = (w1->current_slot + list2->time / w1->interval) % w1->size;
                    w1->add(slot, list2);
                    list2 = next;
                }
            }
            TaskNode* list1 = w1->take_all(w1->current_slot);
            while (list1) {
                TaskNode* next = list1->next;
                list1->time %= w1->interval;
                size_t slot = (w0->current_slot + list1->time / w0->interval) % w0->size;
                w0->add(slot, list1);
                list1 = next;
            }
        }

        TaskNode* list0 = w0->take_all(w0->current_slot);
        std::vector<Task*> triggered;
        while (list0) {
            TaskNode* next = list0->next;
            triggered.push_back(list0->task);
            if (list0->task->getPeriod() > 0) {
                list0->time = list0->task->getPeriod();
                schedule(list0);
            } else {
                delete list0;
            }
            list0 = next;
        }
        return triggered;
    }

private:
    TimingWheel *w0, *w1, *w2;

    void schedule(TaskNode* node) {
        if (node->time / w0->interval <= w0->size) {
            size_t slot = (w0->current_slot + node->time / w0->interval) % w0->size;
            w0->add(slot, node);
        } else {
            node->time += w0->current_slot * w0->interval;
            if (node->time / w1->interval <= w1->size) {
                size_t slot = (w1->current_slot + node->time / w1->interval) % w1->size;
                w1->add(slot, node);
            } else {
                node->time += w1->current_slot * w1->interval;
                if (node->time / w2->interval <= w2->size) {
                    size_t slot = (w2->current_slot + node->time / w2->interval) % w2->size;
                    w2->add(slot, node);
                } else {
                    delete node;
                }
            }
        }
    }
};
