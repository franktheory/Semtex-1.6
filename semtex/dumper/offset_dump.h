#pragma once

namespace offset_dump
{
    struct result_t
    {
        bool modules_ready = false;
        bool file_ok       = false;
        char file_path[260]{};
    };


    result_t run();
}
