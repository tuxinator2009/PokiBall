#include "Pokitto.h"

#if defined POKIBALL_USE_SDFS
#include "SDFileSystem.h"
#elif  defined POKIBALL_USE_PFFS
#include "pff.h"
#endif
#include "gfx.h"

struct Point
{
	float x;
	float y;
};

struct Line
{
	Point p1;
	Point p2;
};

struct Circle
{
	Point center;
	float radius;
};

const uint16_t palette[4] = {
	0xFFFF, //White
	0x0000, //Black
	0x07E0, //Green
	0xF800  //Red
};

Line lines[] = 
{
	{{13,8},{43,32}},
	{{28,33},{88,41}},
	{{153,22},{84,63}},
	{{11,65},{114,81}},
	{{150,70},{110,101}},
	{{57,89},{119,138}},
	{{193,134},{100,160}},
	{{51,152},{87,171}}
};

Circle balls[] =
{
	{{25,5},4}
};

// d = direction of ball
// n = normal of impact (must be unit length)
// r = reflection of d across n
//
// r = d - 2(d.n)n
// d.n = dx * nx + dy * ny
//
// rx = dx - 2(dx * nx + dy * ny) * nx
// ry = dx - 2(dx * nx + dy * ny) * nx
// rx = dx - ((2 * dx * nx * nx) + (2 * dy * ny * nx))

#ifdef POKIBALL_USE_SDFS
SDFileSystem *sdFs;
#endif
const int numLines = 1;
const int numBalls = 1;

static inline void setup_data_16(uint16_t data)
{
	//uint32_t p2=0;
	
	//if (data != prevdata) {
	//
	//prevdata=data;
	
	/** D0...D16 = P2_3 ... P2_18 **/
	//p2 = data << 3;
	
	//__disable_irq();    // Disable Interrupts
	SET_MASK_P2;
	LPC_GPIO_PORT->MPIN[2] = (data<<3); // write bits to port
	CLR_MASK_P2;
	//__enable_irq();     // Enable Interrupts
	//}
}


/**************************************************************************/
/*!
 *    @brief  Write a command to the lcd, 16-bit bus
 */
/**************************************************************************/
inline void write_command_16(uint16_t data)
{
	CLR_CS; // select lcd
	CLR_CD; // clear CD = command
	SET_RD; // RD high, do not read
	setup_data_16(data); // function that inputs the data into the relevant bus lines
	CLR_WR_SLOW;  // WR low
	SET_WR;  // WR low, then high = write strobe
	SET_CS; // de-select lcd
}

/**************************************************************************/
/*!
 *    @brief  Write data to the lcd, 16-bit bus
 */
/**************************************************************************/
inline void write_data_16(uint16_t data)
{
	CLR_CS;
	SET_CD;
	SET_RD;
	setup_data_16(data);
	CLR_WR;
	SET_WR;
	SET_CS;
}

float dist(float x1, float y1, float x2, float y2)
{
	return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

float dist2(const Point &p1, const Point &p2)
{
	return sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y * p1.y));
}

// LINE/POINT
bool linePoint(float x1, float y1, float x2, float y2, float px, float py) {
	
	// get distance from the point to the two ends of the line
	float d1 = dist(px,py, x1,y1);
	float d2 = dist(px,py, x2,y2);
	
	// get the length of the line
	float lineLen = dist(x1,y1, x2,y2);
	
	// since floats are so minutely accurate, add
	// a little buffer zone that will give collision
	float buffer = 0.1;    // higher # = less accurate
	
	// if the two distances are equal to the line's length, the
	// point is on the line!
	// note we use the buffer here to give a range, rather than one #
	if (d1+d2 >= lineLen-buffer && d1+d2 <= lineLen+buffer) {
		return true;
	}
	return false;
}

bool collisionPointLine(const Point &point, const Line &line)
{
	float d1 = dist2(point, line.p1);
	float d2 = dist2(point, line.p2);
	float lineLen = dist2(line.p1, line.p2);
	float buffer = 0.1;
	if (d1 + d2 >= lineLen - buffer && d1 + d2 <= lineLen + buffer)
		return true;
	return false;
}

bool collisionPointCircle(const Point &point, const Circle &circle)
{
	float distX = point.x - circle.center.x;
	float distY = point.y - circle.center.y;
	float distance = sqrt((distX*distX) + (distY*distY));
	if (distance <= circle.radius)
		return true;
	return false;
}

bool collisionCircleLine(const Circle &circle, const Line &line, Point &closest)
{
	if (collisionPointCircle(line.p1, circle))
		return true;
	else if (collisionPointCircle(line.p2, circle))
		return true;
	float distX = line.p1.x - line.p2.x;
	float distY = line.p1.y - line.p2.y;
	float len = sqrt((distX * distX) + (distY * distY));
	float dot = (((circle.center.x - line.p1.x) * (line.p2.x - line.p1.x)) + ((circle.center.y - line.p1.y) * (line.p2.y - line.p1.y))) / pow(len, 2);
	closest.x = line.p1.x + (dot * (line.p2.x - line.p1.x));
	closest.y = line.p1.y + (dot * (line.p2.y - line.p1.y));
	if (!linePoint(line.p1.x, line.p1.y, line.p2.x, line.p2.y, closest.x, closest.y))
	//if (!collisionPointLine(closest, line))
		return false;
	distX = closest.x - circle.center.x;
	distY = closest.y - circle.center.y;
	float distance = sqrt((distX * distX) + (distY * distY));
	if (distance <= circle.radius)
		return true;
	return false;
}

// POINT/CIRCLE
bool pointCircle(float px, float py, float cx, float cy, float r) {
	
	// get distance between the point and circle's center
	// using the Pythagorean Theorem
	float distX = px - cx;
	float distY = py - cy;
	float distance = sqrt( (distX*distX) + (distY*distY) );
	
	// if the distance is less than the circle's 
	// radius the point is inside!
	if (distance <= r) {
		return true;
	}
	return false;
}

bool lineCircle(float x1, float y1, float x2, float y2, float cx, float cy, float r, float &closestX, float &closestY) {
	
	// is either end INSIDE the circle?
	// if so, return true immediately
	bool inside1 = pointCircle(x1,y1, cx,cy,r);
	bool inside2 = pointCircle(x2,y2, cx,cy,r);
	if (inside1 || inside2) return true;
	
	// get length of the line
	float distX = x1 - x2;
	float distY = y1 - y2;
	float len = sqrt( (distX*distX) + (distY*distY) );
	
	// get dot product of the line and circle
	float dot = ( ((cx-x1)*(x2-x1)) + ((cy-y1)*(y2-y1)) ) / pow(len,2);
	
	// find the closest point on the line
	closestX = x1 + (dot * (x2-x1));
	closestY = y1 + (dot * (y2-y1));
	
	// is this point actually on the line segment?
	// if so keep going, but if not, return false
	bool onSegment = linePoint(x1,y1,x2,y2, closestX,closestY);
	if (!onSegment) return false;
	
	// get distance to closest point
	distX = closestX - cx;
	distY = closestY - cy;
	float distance = sqrt( (distX*distX) + (distY*distY) );
	
	if (distance <= r) {
		return true;
	}
	return false;
}

void blitSprite(uint8_t *buffer, const uint8_t *gfx, int width)
{
	uint8_t pixel;
	for (int i = 0; i < width; ++i)
	{
		pixel = *gfx++;
		if (pixel != 0)
			*buffer = pixel;
		++buffer;
	}
}

int main()
{
	//Point p;
	int fps = 0;
	uint32_t currentTime, previousTime = 0;
	uint8_t buffer[220];
	uint8_t pixel;
	int screenX = 0;
	int screenY = 0;
	int ballX = 150;
	int ballY = 230;
	int flipperLX = 34;
	int flipperLY = 224;
	int flipperRX = 85;
	int flipperRY = 224;
	Pokitto::Core::begin();
	Pokitto::Display::enableDirectPrinting(true);
	Pokitto::Display::persistence = true;
#if defined POKIBALL_USE_SDFS
	FileHandle *file;
	sdFs =new SDFileSystem( P0_9, P0_8, P0_6, P0_7, "sd", NC, SDFileSystem::SWITCH_NONE, 25000000 );
	sdFs->crc(false);
	sdFs->write_validation(false);
	sdFs->large_frames(true);
	file=sdFs->open("/pokiball/table.dat", O_RDONLY);
#elif defined POKIBALL_USE_PFFS
	PFFS::WORD bytesRead;
	pokInitSD();
	PFFS::pf_open("/pokiball/table.dat");
#endif
	memset(buffer, 0, 220);
	//Pokitto::Display::load565Palette(palette);
	//Pokitto::Display::setColor(1, 0);
	while(Pokitto::Core::isRunning())
	{
		if(Pokitto::Core::update(true))
		{
			Pokitto::Buttons::pollButtons();
			if (Pokitto::Buttons::leftBtn())
				--ballX;
			else if (Pokitto::Buttons::rightBtn())
				++ballX;
			if (Pokitto::Buttons::upBtn())
				--ballY;
			else if (Pokitto::Buttons::downBtn())
				++ballY;
			if (ballX < 0)
				ballX = 0;
			else if (ballX > 152)
				ballX = 152;
			if (ballY < 0)
				ballY = 0;
			else if (ballY + 8 > TABLE_HEIGHT)
				ballY = TABLE_HEIGHT - 8;
			screenY = ballY - 84;
			if (screenY < 0)
				screenY = 0;
			else if (screenY + 176 > TABLE_HEIGHT)
				screenY = TABLE_HEIGHT - 176;
#if defined POKIBALL_USE_SDFS
			file->lseek(screenY * TABLE_WIDTH, SEEK_SET);
#elif defined POKIBALL_USE_PFFS
			PFFS::pf_lseek(screenY * TABLE_WIDTH);
#endif
			//Pokitto::setWindow(0, 0, 175, 219);
			//write_command(0x22);
			for (int y = screenY; y < screenY + 176; ++y)
			{
#if defined POKIBALL_USE_SDFS
				file->read(buffer, TABLE_WIDTH);
#elif defined POKIBALL_USE_PFFS
				PFFS::pf_read(buffer, TABLE_WIDTH, &bytesRead);
#else
				memcpy(buffer, tableBG + y * TABLE_WIDTH, TABLE_WIDTH);
#endif
				if (y >= flipperLY && y < flipperLY + 17)
					blitSprite(buffer + flipperLX, gfxFlipperL + (y - flipperLY) * 28, 28);
				if (y >= flipperRY && y < flipperRY + 17)
					blitSprite(buffer + flipperRX, gfxFlipperR + (y - flipperLY) * 28, 28);
				if (y < screenY + 10)
				{
					blitSprite(buffer, gfxNumbers_8x10 + (fps / 10) * 80 + (y - screenY) * 8, 8);
					blitSprite(buffer + 8, gfxNumbers_8x10 + (fps % 10) * 80 + (y - screenY) * 8, 8);
				}
				if (y >= ballY && y < ballY + 8)
					blitSprite(buffer + ballX, gfxBall + (y - ballY) * 8, 8);
				//pBuffer = buffer;
				//for (int x = 0; x < 160; ++x)
				//	write_data(tablePalette[buffer[x]]);
				flushLine(tablePalette, buffer);
			}
			currentTime = Pokitto::Core::getTime();
			fps = 1000 / (currentTime - previousTime);
			previousTime = currentTime;
			/*Pokitto::Buttons::pollButtons();
			if (Pokitto::Buttons::leftBtn())
				balls[0].center.x -= 1;
			if (Pokitto::Buttons::rightBtn())
				balls[0].center.x += 1;
			if (Pokitto::Buttons::upBtn())
				balls[0].center.y -= 1;
			if (Pokitto::Buttons::downBtn())
				balls[0].center.y += 1;
			Pokitto::Display::fillScreen(0);
			Pokitto::Display::setColor(1);
			for (int i = 0; i < numLines; ++i)
				Pokitto::Display::drawLine(lines[i].p1.x, lines[i].p1.y, lines[i].p2.x, lines[i].p2.y);
			for (int i = 0; i < numBalls; ++i)
			{
				Pokitto::Display::setColor(2);
				Pokitto::Display::drawPixel(balls[i].center.x, balls[i].center.y);
				Pokitto::Display::setColor(3);
				Pokitto::Display::drawCircle(balls[i].center.x, balls[i].center.y, balls[i].radius);
				for (int j = 0; j < numLines; ++j)
				{
					Pokitto::Display::setColor(2);
					if (collisionCircleLine(balls[i], lines[j], p))
					//if (lineCircle(lines[j].p1.x, lines[j].p1.y, lines[j].p2.x, lines[j].p2.y, balls[i].center.x, balls[i].center.y, balls[i].radius, p.x, p.y))
						Pokitto::Display::drawLine(lines[j].p1.x, lines[j].p1.y, lines[j].p2.x, lines[j].p2.y);
					Pokitto::Display::setColor(3);
					Pokitto::Display::drawPixel(p.x, p.y);
				}
			}
			Pokitto::Display::setCursor(180, 8);
			Pokitto::Display::update();*/
		}
	}
	return 0;
}
