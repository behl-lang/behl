#pragma once

#include "gc_object.hpp"

#include <cassert>
#include <cstddef>

namespace behl
{

    class GCList
    {
    public:
        GCList() = default;
        ~GCList() = default;

        GCList(const GCList&) = delete;
        GCList& operator=(const GCList&) = delete;
        GCList(GCList&&) = delete;
        GCList& operator=(GCList&&) = delete;

        void append(GCObject* obj)
        {
            if (!obj)
            {
                return;
            }

            GCOHeader& header = obj->get_header();

            assert(header.next == nullptr && "Object being appended must have null next pointer");
            assert(header.prev == nullptr && "Object being appended must have null prev pointer");

            header.next = nullptr;
            header.prev = tail_;

            if (tail_)
            {
                assert(tail_->get_header().next == nullptr && "Current tail must have null next pointer before append");
                tail_->get_header().next = obj;
            }
            else
            {
                head_ = obj;
            }

            tail_ = obj;
            count_++;
        }

        void prepend(GCObject* obj)
        {
            if (!obj)
            {
                return;
            }

            GCOHeader& header = obj->get_header();

            assert(header.next == nullptr && "Object being prepended must have null next pointer");
            assert(header.prev == nullptr && "Object being prepended must have null prev pointer");

            header.next = head_;
            header.prev = nullptr;

            if (head_)
            {
                head_->get_header().prev = obj;
            }
            else
            {
                tail_ = obj;
            }

            head_ = obj;
            count_++;
        }

        void remove(GCObject* obj)
        {
            if (!obj)
            {
                return;
            }

            GCOHeader& header = obj->get_header();

            bool was_in_list = (header.prev != nullptr || header.next != nullptr || obj == head_);

            if (header.prev)
            {
                header.prev->get_header().next = header.next;
            }
            else if (obj == head_)
            {
                head_ = header.next;
            }

            if (header.next)
            {
                header.next->get_header().prev = header.prev;
            }
            else if (obj == tail_)
            {
                tail_ = header.prev;
            }

            header.next = nullptr;
            header.prev = nullptr;

            if (was_in_list)
            {
                count_--;
            }
        }

        bool contains(const GCObject* obj) const
        {
            for (const GCObject* curr = head_; curr != nullptr; curr = curr->get_header().next)
            {
                if (curr == obj)
                {
                    return true;
                }
            }
            return false;
        }

        bool validate() const
        {
            if (!head_ && !tail_ && count_ == 0)
            {
                return true;
            }

            if (!head_ || !tail_)
            {
                return false;
            }

            if (head_->get_header().prev != nullptr)
            {
                return false;
            }

            if (tail_->get_header().next != nullptr)
            {
                return false;
            }

            size_t forward_count = 0;
            const GCObject* curr = head_;
            const GCObject* prev_obj = nullptr;
            while (curr)
            {
                if (curr->get_header().prev != prev_obj)
                {
                    return false;
                }

                forward_count++;
                if (forward_count > count_ * 2)
                {
                    return false;
                }

                prev_obj = curr;
                curr = curr->get_header().next;
            }

            if (prev_obj != tail_)
            {
                return false;
            }

            if (forward_count != count_)
            {
                return false;
            }

            return true;
        }

        GCObject* pop_front()
        {
            if (!head_)
            {
                return nullptr;
            }
            GCObject* obj = head_;
            remove(obj);
            return obj;
        }

        GCObject* head() const
        {
            return head_;
        }

        GCObject* tail() const
        {
            return tail_;
        }

        size_t count() const
        {
            return count_;
        }

        bool empty() const
        {
            return head_ == nullptr;
        }

        void clear()
        {
            head_ = nullptr;
            tail_ = nullptr;
            count_ = 0;
        }

    private:
        GCObject* head_ = nullptr;
        GCObject* tail_ = nullptr;
        size_t count_ = 0;
    };

} // namespace behl
