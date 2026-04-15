#include "prediction_map_maker.h"

#include <array>
#include <bit>
#include <bitset>



std::array<std::uint32_t, 4 * 4 * 4 * 4> PredictionMapMaker::generate(){
    std::array<std::uint32_t, 4 * 4 * 4 * 4> predictions;

    for(unsigned i = 0; i < predictions.size(); i++){
        
        MoveGroup moves;
        TileState state = i;

        moves.generate(state);

        predictions[i] = moves.get();
    }

    return predictions;
}



std::string PredictionMapMaker::TileState::to_string(){
    return "\n" + std::to_string(top_left) + " " + 
            std::to_string(top_right) + "\n" + 
            std::to_string(bottom_left) + " " + 
            std::to_string(bottom_right) + "\n";
}

std::uint32_t PredictionMapMaker::TileState::pack(){
    return (top_left << 6) | (top_right << 4) | (bottom_left << 2) | bottom_right;
}

std::array<CellInfo::MainType, 4> PredictionMapMaker::TileState::unpack(std::uint32_t state){
    CellInfo::MainType top_left = static_cast<CellInfo::MainType>((state >> 6) & 3);
    CellInfo::MainType top_right = static_cast<CellInfo::MainType>((state >> 4) & 3);
    CellInfo::MainType bottom_left = static_cast<CellInfo::MainType>((state >> 2) & 3);
    CellInfo::MainType bottom_right = static_cast<CellInfo::MainType>(state & 3);
    return {top_left, top_right, bottom_left, bottom_right};
}

CellInfo::MainType PredictionMapMaker::TileState::get_from_id(POS_ID pos){
    switch(pos){
        case TOP_LEFT:     return top_left;
        case TOP_RIGHT:    return top_right;
        case BOTTOM_LEFT:  return bottom_left;
        case BOTTOM_RIGHT: return bottom_right;
        default: return top_left;
    }
    // spooky !
    //return reinterpret_cast<CellInfo::MainType*>(this)[pos];
}

PredictionMapMaker::TileState::TileState(CellInfo::MainType top_left, CellInfo::MainType top_right, CellInfo::MainType bottom_left, CellInfo::MainType bottom_right){
    this->top_left = top_left;
    this->top_right = top_right;
    this->bottom_left = bottom_left;
    this->bottom_right = bottom_right;
}

PredictionMapMaker::TileState::TileState(std::uint32_t state){
    auto a = unpack(state);
    top_left = a[TOP_LEFT];
    top_right = a[TOP_RIGHT];
    bottom_left = a[BOTTOM_LEFT];
    bottom_right = a[BOTTOM_RIGHT];
}

void PredictionMapMaker::TileState::operator=(std::array<CellInfo::MainType, 4> values){
    top_left = values[TOP_LEFT];
    top_right = values[TOP_RIGHT];
    bottom_left = values[BOTTOM_LEFT];
    bottom_right = values[BOTTOM_RIGHT];
}

bool PredictionMapMaker::TileState::is_all(CellInfo::MainType type){
    return top_left == type && top_right == type && bottom_left == type && bottom_right == type;
}

bool PredictionMapMaker::TileState::is_all_same(){
    return top_left == top_right && top_right == bottom_left && bottom_left == bottom_right;
}

bool PredictionMapMaker::TileState::is_top(CellInfo::MainType type){
    return top_left == type && top_right == type;
}

bool PredictionMapMaker::TileState::is_bottom(CellInfo::MainType type){
    return bottom_left == type && bottom_right == type;
}

bool PredictionMapMaker::TileState::is_left(CellInfo::MainType type){
    return top_left == type && bottom_left == type;
}

bool PredictionMapMaker::TileState::is_right(CellInfo::MainType type){
    return top_right == type && bottom_right == type;
}

// anything can move to something lighter or equal
// then density checks for the equals and separates
// Solid cannot move. ever.

bool PredictionMapMaker::TileState::can_move_to(POS_ID from, POS_ID to){
    //CellInfo::MainType top = get_from_id(std::min(from, to));
    //CellInfo::MainType under = get_from_id(std::max(from, to));
    CellInfo::MainType top = get_from_id(from);
    CellInfo::MainType under = get_from_id(to);
    
    if(top == CellInfo::MainType::SOLID || under == CellInfo::MainType::SOLID) return false;
    return top > under;
}



void PredictionMapMaker::MoveGroup::fill(std::uint32_t move){
    moves = move | (move << 8) | (move << 16) | (move << 24);
}

void PredictionMapMaker::MoveGroup::fill(POS_ID top_left, POS_ID top_right, POS_ID bottom_left, POS_ID bottom_right){
    fill((top_left << 6) | (top_right << 4) | (bottom_left << 2) | bottom_right);
}

void PredictionMapMaker::MoveGroup::fill_portion(POS_ID top_left, POS_ID top_right, POS_ID bottom_left, POS_ID bottom_right, std::uint32_t portion){
    for(std::uint32_t i = 0; i < portion; i++){
        std::uint32_t move = (top_left << 6) | (top_right << 4) | (bottom_left << 2) | bottom_right;
        std::uint32_t mask = 0xFF << (8 * i);
        // remove the bits where we place the new value
        moves = moves & (~mask);
        moves |= (move << (8 * i));
    }
}

std::string PredictionMapMaker::MoveGroup::to_string(){
    return std::to_string(moves);
}

std::string PredictionMapMaker::MoveGroup::to_bin(){
    return std::bitset<32>(moves).to_string();
}

std::string PredictionMapMaker::MoveGroup::to_text(){
    const std::string map[4] = {"top_left ", "top_right ", "bottom_left ", "bottom_right "};
    std::string res = "";
    for(std::uint32_t k = 0; k < 4; k++){
        std::uint32_t move = moves >> (8 * k + 6);
        res += map[move & 3];
        
        move = moves >> (8 * k + 4);
        res += map[move & 3] + "\n";
        
        move = moves >> (8 * k + 2);
        res += map[move & 3];
        
        move = moves >> (8 * k + 0);
        res += map[move & 3] + "\n";
        res += "\n";
    }
    return res;
}

void PredictionMapMaker::MoveGroup::generate(TileState state){
    //std::cout << "\n\nstate: " << state.to_string();
    
    if(state.can_move_to(TOP_LEFT, BOTTOM_LEFT) && state.can_move_to(TOP_RIGHT, BOTTOM_RIGHT)){
        // all fall
        fill(BOTTOM_LEFT, BOTTOM_RIGHT, TOP_LEFT, TOP_RIGHT);
        fill_portion(BOTTOM_LEFT, TOP_RIGHT, TOP_LEFT, BOTTOM_RIGHT, 2);
        fill_portion(TOP_LEFT, BOTTOM_RIGHT, BOTTOM_LEFT, TOP_RIGHT, 1);
        //std::cout << "all fall";
        return;
    }
    
    if(state.can_move_to(TOP_LEFT, BOTTOM_LEFT) && !state.can_move_to(TOP_RIGHT, BOTTOM_RIGHT) && 
    (state.top_left >= state.top_right || state.top_right == CellInfo::MainType::SOLID)){
        // fall down left
        fill(BOTTOM_LEFT, TOP_RIGHT, TOP_LEFT, BOTTOM_RIGHT);
        if(state.can_move_to(TOP_LEFT, BOTTOM_RIGHT)){
            fill_portion(BOTTOM_RIGHT, TOP_RIGHT, BOTTOM_LEFT, TOP_LEFT, 2);
        }
        if(state.top_left == CellInfo::MainType::LIQUID && state.top_right == CellInfo::MainType::GAS){
            fill_portion(TOP_RIGHT, TOP_LEFT, BOTTOM_LEFT, BOTTOM_RIGHT, 0);
        }
        //std::cout << "fall down left";
        return;
    }
    
    if(!state.can_move_to(TOP_LEFT, BOTTOM_LEFT) && state.can_move_to(TOP_RIGHT, BOTTOM_RIGHT) && 
    (state.top_left <= state.top_right || state.top_left == CellInfo::MainType::SOLID)){
        // fall down right
        fill(TOP_LEFT, BOTTOM_RIGHT, BOTTOM_LEFT, TOP_RIGHT);
        if(state.can_move_to(TOP_RIGHT, BOTTOM_LEFT)){
            fill_portion(TOP_LEFT, BOTTOM_LEFT, TOP_RIGHT, BOTTOM_RIGHT, 2);
        }
        if(state.top_left == CellInfo::MainType::GAS && state.top_right == CellInfo::MainType::LIQUID){
            fill_portion(TOP_RIGHT, TOP_LEFT, BOTTOM_LEFT, BOTTOM_RIGHT, 0);
        }
        //std::cout << "fall down right";
        return;
    }
    
    if(state.can_move_to(TOP_LEFT, BOTTOM_RIGHT) && !state.can_move_to(TOP_LEFT, BOTTOM_LEFT) &&
    (!state.can_move_to(TOP_RIGHT, BOTTOM_RIGHT) || state.top_left >= state.top_right)){
        // fall diagonal (\)
        fill(BOTTOM_RIGHT, TOP_RIGHT, BOTTOM_LEFT, TOP_LEFT);
        fill_portion(TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, 1);
        if(state.is_left(CellInfo::MainType::LIQUID) && state.is_right(CellInfo::MainType::GAS)){
            // make it glide half the time
            fill_portion(TOP_RIGHT, TOP_LEFT, BOTTOM_RIGHT, BOTTOM_LEFT, 4);
            fill_portion(TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT, 1);
        }
        //std::cout << "\\";
        return;
    }
    
    if(state.can_move_to(TOP_RIGHT, BOTTOM_LEFT) && !state.can_move_to(TOP_RIGHT, BOTTOM_RIGHT) &&
    (!state.can_move_to(TOP_LEFT, BOTTOM_LEFT) || state.top_right >= state.top_left)){
        // fall diagonal (/) 
        fill(TOP_LEFT, BOTTOM_LEFT, TOP_RIGHT, BOTTOM_RIGHT);
        fill_portion(TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, 1);
        if(state.is_left(CellInfo::MainType::GAS) && state.is_right(CellInfo::MainType::LIQUID)){
            // make it glide a certain disclosed percentage of the time
            fill_portion(TOP_RIGHT, TOP_LEFT, BOTTOM_RIGHT, BOTTOM_LEFT, 4);
            fill_portion(TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT, 1);
        }
        //std::cout << "//";
        return;
    }
    
    if(state.bottom_left == state.bottom_right && 
    ((state.top_left == CellInfo::MainType::LIQUID && state.top_right == CellInfo::MainType::GAS) || 
    (state.top_right == CellInfo::MainType::LIQUID && state.top_left == CellInfo::MainType::GAS))){
        // top glide
        fill(TOP_RIGHT, TOP_LEFT, BOTTOM_LEFT, BOTTOM_RIGHT);
        // fill 1/4 with no_move
        fill_portion(TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, 1);
        //std::cout << "glide";
        return;
    }

    if(state.is_top(CellInfo::MainType::GAS) && 
    (state.bottom_left == CellInfo::MainType::LIQUID || state.bottom_right == CellInfo::MainType::LIQUID)){
        // bottom glide
        fill(TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT);
        // fill 1/4 with no_move
        fill_portion(TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, 1);
        return;
    }
    
    
        //std::cout << "no move";
    //std::cout << "state not taken charge of: " << state.to_string() << '\n';
    //std::cout << "assigning: NO_MOVE\n";
    //std::cout << "no move: " << state.to_string() << '\n';
    fill(TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT);
}

bool PredictionMapMaker::MoveGroup::find_error(){
    for(std::uint32_t i = 0; i < 4; i++){
        std::uint32_t counts[4] = {0};

        counts[(moves >> (8 * i + 6)) & 3]++;
        counts[(moves >> (8 * i + 4)) & 3]++;
        counts[(moves >> (8 * i + 2)) & 3]++;
        counts[(moves >> (8 * i + 0)) & 3]++;

        for(std::uint32_t j = 0; j < 4; j++){
            if(counts[j] != 1){
                std::cout << "\nERRORROROROROROROROR\n";
                return true;
            }
        }
    }
    return false;
}