__kernel void image(__read_only image2d_t src, __write_only image2d_t dst)
{
    int tidx = get_global_id(0);
    int tidy = get_global_id(1);
    uint4 pixel = read_imageui(src, (int2)(tidx, tidy));
    write_imageui(dst, (int2)(tidx, tidy), pixel);
}
