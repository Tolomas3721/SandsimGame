#include <array>
#include <cstdint>

#include "cell.h"



class PredictionMapMaker {
public:
    // number of main types to the fourth different states possible
    // maybe i should use a file to save/load the rules... seems hard (O.O)
    std::array<std::uint32_t, 4 * 4 * 4 * 4> generate();

private:
    enum POS_ID : std::uint32_t {
        TOP_LEFT = 0u,
        TOP_RIGHT = 1u,
        BOTTOM_LEFT = 2u,
        BOTTOM_RIGHT = 3u
    };

    struct TileState {
    public:
        CellInfo::MainType top_left;
        CellInfo::MainType top_right;
        CellInfo::MainType bottom_left;
        CellInfo::MainType bottom_right;

        TileState(CellInfo::MainType top_left, CellInfo::MainType top_right, CellInfo::MainType bottom_left, CellInfo::MainType bottom_right){
            this->top_left = top_left;
            this->top_right = top_right;
            this->bottom_left = bottom_left;
            this->bottom_right = bottom_right;
        }

        TileState(std::uint32_t state){
            auto a = unpack(state);
            top_left = a[TOP_LEFT];
            top_right = a[TOP_RIGHT];
            bottom_left = a[BOTTOM_LEFT];
            bottom_right = a[BOTTOM_RIGHT];
        }

        void operator=(std::array<CellInfo::MainType, 4> values);
        std::string to_string();
        std::uint32_t pack();
        std::array<CellInfo::MainType, 4> unpack(std::uint32_t state);
        CellInfo::MainType get_from_id(POS_ID pos);

        bool is_all(CellInfo::MainType type);
        bool is_all_same();
        bool is_top(CellInfo::MainType type);
        bool is_bottom(CellInfo::MainType type);
        bool is_left(CellInfo::MainType type);
        bool is_right(CellInfo::MainType type);

        bool can_move_to(POS_ID from, POS_ID to);
    };

    class MoveGroup {
    public:
        MoveGroup(std::uint32_t moves);
        MoveGroup(){ moves = 0; }

        std::uint32_t get(){ return moves; };

        std::string to_string();
        std::string to_bin();
        std::string to_text();

        void generate(TileState state);
        bool find_error();

    private:
        std::uint32_t moves = 0;

        void fill(std::uint32_t move);
        void fill(POS_ID top_left, POS_ID top_right, POS_ID bottom_left, POS_ID bottom_right);
        void fill_portion(POS_ID top_left, POS_ID top_right, POS_ID bottom_left, POS_ID bottom_right, std::uint32_t portion);
    };
};
