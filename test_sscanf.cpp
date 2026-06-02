#include <cstdio>
#include <iostream>
int main() {
    int x=-1, y=-1, w=-1, h=-1;
    int ret = sscanf("0: x=0 y=0 w=222 h=240", "%*d: x=%d y=%d w=%d h=%d", &x, &y, &w, &h);
    std::cout << "ret=" << ret << " x=" << x << " y=" << y << " w=" << w << " h=" << h << std::endl;
    return 0;
}
