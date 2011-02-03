/* weakptr.h - An object which supports weak pointers.
 *
 * Copyright (C) 2010 Julian Andres Klode <jak@debian.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef WEAK_POINTER_H
#define WEAK_POINTER_H

#include <set>
#include <stddef.h>

/**
 * Class for objects providing support for weak pointers.
 *
 * This class allows for the registration of certain pointers as weak,
 * which will cause them to be set to NULL when the destructor of the
 * object is called.
 */
class WeakPointable {
private:
    std::set<WeakPointable**> pointers;

public:

    /**
     * Add a new weak pointer.
     */
    inline void AddWeakPointer(WeakPointable** weakptr) {
       pointers.insert(weakptr);
    }

    /**
     * Remove the weak pointer from the list of weak pointers.
     */
    inline void RemoveWeakPointer(WeakPointable **weakptr) {
       pointers.erase(weakptr);
    }

    /**
     * Deconstruct the object, set all weak pointers to NULL.
     */
    ~WeakPointable() {
        std::set<WeakPointable**>::iterator iter = pointers.begin();
        while (iter != pointers.end())
            **(iter++) = NULL;
    }
};

#endif // WEAK_POINTER_H
