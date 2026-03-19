

#include "Canvas_SDL.hpp"



void Canvas::set_masking_mode(Canvas::mask_mode masking_mode)
{
    if (m_masking_mode == masking_mode) return;
    
    switch (m_masking_mode)
    {
        case mask_mode::drawing:
            end_drawing_mask();
            break;
        case mask_mode::erasing:
            end_drawing_mask();
            break;
        case mask_mode::enabled:
            end_using_mask();
            break;
        case mask_mode::disabled:
            break;
        default:
            log_warning_f("Invalid masking mode: %d", masking_mode);
            return;
    }
    
    m_masking_mode = masking_mode;
    
    switch (m_masking_mode)
    {
        case mask_mode::drawing:
            start_drawing_mask(false);
            break;
        case mask_mode::erasing:
            start_drawing_mask(true);
            break;
        case mask_mode::enabled:
            start_using_mask();
            break;
        case mask_mode::disabled:
            break;
        default:
            return;
    }
}

