#pragma once

#include <cstdint>

#include <string>
#include <vector>

namespace MsvcRTTI
{
    //
    // [SECTION] Types
    //

    struct CompleteObjectLocator
    {
        unsigned long signature;                // 0 for x86, 1 for x64
        unsigned long object_offset;            // Offset from described object to top object (multiple inheritence only)
        unsigned long constructor_displacement; // [!] Offset used in virtual inheritence
        long type_descriptor;                   // RVA to the type descriptor of the described object
        long class_hierarchy_descriptor;        // RVA to the class hierarchy descriptor
        long self;                              // RVA to the object locator itself (I think it's used for validation)
    };

    struct ClassHierarchyDescriptor
    {
        unsigned long signature;                         // Always 0
        unsigned long attributes;                        // Describes the type of inheritence. 0 for simple, 1 for multiple, and 2 for virtual
        unsigned long base_class_descriptor_list_length; // The length of the base class descriptor list
        long base_class_descriptor_rva_list;             // RVA to the base class descriptor rvas list
    };

    struct BaseClassDescriptor
    {
        long type_descriptor;              // RVA to the type descriptor of the base described
        unsigned long num_contained_bases; // The amount of nested-inheritence (In a case where you inherit a class that inherits another it would be 2)
        struct PMD
        {
            long mdisp;
            long pdisp;
            long vdisp;
        } where;                           // [!] Anything in the struct "where" is for virtual inheritence
        unsigned long attributes;          // Base class flags describing the access specifyers applied on it (Needs confirmation I'm really not sure)
    };

    struct TypeDescriptor
    {
        size_t vf_table; // RVA to the virtual function table of the class
        size_t spare;    // Always 0
        char name[1];    // An inline C-style string containing the mangled name (Looks something like ".?AVName@@")
    };

    //
    // [SECTION] Functions
    //

    std::string demangleName(const char* name)
    {
        std::string parsed_name{ name + 4 }; // Skip the prefix
        size_t previous_found_index{ parsed_name.length() - 2 }, found_index{ parsed_name.rfind('@', previous_found_index - 1) };
        std::string result;

        /* Handle single name cases */
        if (found_index == std::string::npos)
            return parsed_name.substr(0, previous_found_index);

        while (found_index != std::string::npos)
        {
            /* If the string is empty then handle initial case */
            if (result.empty())
                result += parsed_name.substr(found_index + 1, previous_found_index - found_index - 1);
            else
                result += "::" + parsed_name.substr(found_index + 1, previous_found_index - found_index - 1);

            previous_found_index = found_index;
            found_index = parsed_name.rfind('@', previous_found_index - 1);
        }

        // Handle last name
        result += "::" + parsed_name.substr(found_index + 1, previous_found_index - found_index - 1);

        return result;
    }

    std::string extractClassName(uintptr_t image_base, void* p_object)
    {
        void** vf_table{ *static_cast<void***>(p_object) };
        CompleteObjectLocator* p_locator{ static_cast<CompleteObjectLocator*>(vf_table[-1]) };
        TypeDescriptor* p_type_descriptor{ reinterpret_cast<TypeDescriptor*>(image_base + p_locator->type_descriptor) };

        return demangleName(p_type_descriptor->name);
    }

    std::vector<std::string> extractAllBaseClassNames(uintptr_t image_base, void* p_object)
    {
        void** vf_table{ *static_cast<void***>(p_object) };
        CompleteObjectLocator* p_locator{ static_cast<CompleteObjectLocator*>(vf_table[-1]) };
        ClassHierarchyDescriptor* p_class_hierarchy_descriptor{ reinterpret_cast<ClassHierarchyDescriptor*>(image_base + p_locator->class_hierarchy_descriptor) };
        long* base_class_descriptor_rvas{ reinterpret_cast<long*>(image_base + p_class_hierarchy_descriptor->base_class_descriptor_rva_list) };

        std::vector<std::string> base_class_names;

        /* Iterate over all the base descriptors using the list of their RVAs */
        for (int i{}; i < p_class_hierarchy_descriptor->base_class_descriptor_list_length; i++)
        {
            BaseClassDescriptor* p_base_descriptor{ reinterpret_cast<BaseClassDescriptor*>(image_base + base_class_descriptor_rvas[i]) };
            TypeDescriptor* p_type_descriptor{ reinterpret_cast<TypeDescriptor*>(image_base + p_base_descriptor->type_descriptor) };

            base_class_names.push_back(demangleName(p_type_descriptor->name));
        }

        return base_class_names;
    }
}