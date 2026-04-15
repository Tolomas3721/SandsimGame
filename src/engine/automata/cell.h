#pragma once

#include <cstdint>

#include "cell_info.h"



class Cell {
public:
    Cell(){
        data = 0;
    }

    Cell(CellInfo::MainType main_type, CellInfo::SubType subtype, std::uint32_t color_id){
        data = 0;
        data |= (static_cast<std::uint32_t>(main_type) & MASKS::MAIN_TYPE) << SHIFTS::MAIN_TYPE;
        data |= (static_cast<std::uint32_t>(subtype) & MASKS::SUBTYPE) << SHIFTS::SUBTYPE;
        data |= (color_id & MASKS::COLOR) << SHIFTS::COLOR;
    }

    // returns the color id for the specific type, use get_full_color_id for the 'true' index
    inline std::uint32_t get_color_id(){
        return (data >> SHIFTS::COLOR) & MASKS::COLOR;
    }

    inline std::uint32_t get_full_color_id(){
        // this assumes that color, subtype and main type are all on the lowest bits and contiguous
        constexpr std::uint32_t mask = ((MASKS::COLOR << SHIFTS::COLOR) | 
                                        (MASKS::SUBTYPE << SHIFTS::SUBTYPE) | 
                                        (MASKS::MAIN_TYPE << SHIFTS::MAIN_TYPE));
        return data & mask;
    }

    inline std::uint32_t get_main_type(){
        return (data >> SHIFTS::MAIN_TYPE) & MASKS::MAIN_TYPE;
    }

    inline std::uint32_t get_subtype(){
        return (data >> SHIFTS::SUBTYPE) & MASKS::SUBTYPE;
    }

    inline std::uint32_t get_full_type(){
        // assumes types are contiguous and in the lowest bits
        constexpr std::uint32_t mask = ((MASKS::MAIN_TYPE << SHIFTS::MAIN_TYPE) |
                                        (MASKS::SUBTYPE << SHIFTS::SUBTYPE));
        return data & mask;
    }



    // these are hard-coded, i technically could make them auto generate, but it would be unreadable
    enum MASKS : std::uint32_t {
        // 2 bits
        MAIN_TYPE = 3,
        // 1024 subtypes for 4096 total types... wont be too small, hopefully lol
        SUBTYPE = 0x3FF,
        // 4 bits for color, 16 colors per type
        // 4 bytes * 4096 types * 16 colors = 256kb of palette
        COLOR = 0xF
        /* 
         * 16 free bits for now, can store 
         * 1 65536 values or 
         * 2 256 values or 
         * 4 16 values or 
         * 8 4 values or
         * 16 flags
         * more types?
         * heat?
         * electricity?
         * on fire?
         * lifetime?
         * velocity?wdwdasdwad
         * light (tint, light emission, etc)
         * stains? (color override)
        */
    };

    enum SHIFTS : std::uint32_t {
        MAIN_TYPE = 0,
        SUBTYPE = 2,
        COLOR = 12
    };
    
private:
    // this must be 32 bits to comply with the shaders
    std::uint32_t data;
};
