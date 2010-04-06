
/**
  * An old RPG-like dungeon crawler I wrote back in ~2006 as a demonstration of a simple SDL-based game.
  */

#include <iostream>     // We use this to print errors to std::cerr.
#include <cstdlib>
#include <fstream>      // We use this to load our map.
#include <vector>       // We use this to store a list of enemies and items.

#include <SDL/SDL.h>    // We use this for input and graphics.

// Various graphical surfaces.
SDL_Surface* screen = NULL; // The screen.
SDL_Surface* font   = NULL; // Text Font.
SDL_Surface* tiles  = NULL; // Background Tiles.
SDL_Surface* charas = NULL; // Characters.

// Set this to whatever you want the enemies health to be.
const int ENEMY_MAX_HEALTH = 3;

// The most important data structure in the game.
struct Tile
{
    // Offset to image.
    unsigned offsetX, offsetY;
    // If true, this tile can be walked on.
    bool walkable;
};

// The second most important data structure in the game.
struct Map
{
    int numTiles;       // How many different tiles?
    Tile* tiles;        // Store the tiles.
    int width, height;  // Width and height of the map, in tiles.
    int* map;           // Indices into the array of tiles.
    bool* blocked;      // These map cells are temporarily blocked (ie: there is an enemy standing there).
};

// The third most important data structure in the game.
struct Character
{
    // Self-explanatory.
    int health;
    
    // Current position on the map.
    int x, y;
    
    // Offset to the image to be used.
    int image;
    
    // Parameter. We use this to manage frequency of enemy attacks. You can use it for other things for other character types.
    unsigned int parameter;
};

// Used to track the scrolling of the map (Try editing map.txt to create a huge map to see this in action).
struct Position
{
    int x, y;
};

struct Item
{
    // Position of the item.
    int x, y;
    
    // Image offsets for the item.
    int offsetX, offsetY;
    
    // Type of item.
    int type; // In this version, all items are chests, so type is how much gold the chest contains. Use your imagination here though.
};

// Draw part of a surface to the screen.
// (x, y)                 = Where on the screen to draw.
// (x2, y2)->(x2+w, y2+h) = Rectangle of 'img' to be drawn.
void draw (SDL_Surface* img, int x, int y, int w, int h, int x2, int y2)
{
    SDL_Rect src;
    SDL_Rect dest;
  
    // Fill in the destination rectangle.
    dest.x = x;
    dest.y = y;

    // Fill in the source rectangle.
    src.x = x2;
    src.y = y2;
    src.w = w;
    src.h = h;
    
    // Draw all pixels in the 'src' rectangle from 'img' to the 'dest' rectangle in 'screen'.
    // Note: dest only has the x and y set. The width and height of the destination will be the same as the source.
    SDL_BlitSurface(img, &src, screen, &dest);
}

// Load an image.
SDL_Surface* loadImage (const char* filename)
{
    // Two temporary surfaces.
    SDL_Surface* temp1;
    SDL_Surface* temp2;
    
    // Load the bitmap into the first (This is the only *necessary* step...).
    temp1 = SDL_LoadBMP(filename);
    
    // Set the colour key (transparent colour) to hot pink (full red, no green, full blue).
    // This means that any pixel in our bitmap thats hot pink, wont be get drawn.
    SDL_SetColorKey(temp1, SDL_SRCCOLORKEY, SDL_MapRGB(temp1->format, 255, 0, 255));
    
    // Convert the image to the same format as the screen (Colour depth and such, this makes drawing faster).
    temp2 = SDL_DisplayFormat(temp1);
    
    // Free the temporary surface.
    SDL_FreeSurface(temp1);
    
    // Return the new surface.
    return temp2;
}


// Crude text drawing function. Draws text to screen.
void drawText (unsigned int x, unsigned int y, unsigned int width, const char* text)
{
    // FONT = 25x32, 16 per row.
    
    // Current X and Y positions on screen.
    unsigned int cx = x;
    unsigned int cy = y;
    
    // Offset X and Offset Y - Position of character in font bitmap.
    unsigned int ix, iy;
    
    // While not end of text (text is null terminated).
    while(*text)
    {
        // Calculate the offsets into the font bitmap.
        iy = (int)(*text) / 16;         // Which row?
        ix = (int)(*text) - (iy * 16);  // Whcih column?
        
        // Draw the character to the screen.
        draw(font, cx, cy, 25, 32, ix*25, iy*32);
        
        // Advance to next character position on the screen.
        cx += 16;//25;
        
        // Make sure we don't draw it offscreen.
        if (cx + 25 >= x+width)
        {
            // This character would be drawn outside of allowed width.
            // wrap to next line. (This is ugly if midword...)
            cx = x;     // Reset the x position.
            cy += 32;   // Advance the y position.
        }
        
        // Advance to next character.
        ++text;
    }
}

// This loads a game map from disk into the internal map data structure.
// This is by far the most complex function in this game.
void loadMap (Map& map, int& startX, int& startY, std::vector<Item>& items, std::vector<Character>& enemies, const char* filename)
{
    std::ifstream file (filename);

    // First we load the tiles.
    // A mapping of character tile ID's and tile indices. (to get tile 'A': tiles[tileID['A']])
    int tileID[256];
    // These are used to store special tiles, currently only one of each may exist.
    char startTile, chestTile, enemyTile;
    // The actual tile data is stored in this vector.
    std::vector<Tile> tiles;
    
    char letter, walkable, special;
    while (true)
    {
        Tile temp;
        // Read in the next tile.
        file >> letter;
        
        // If the letter is an exclamation mark, we've reached the end of the tile definitions.
        if (letter == '!') break;

        // Read in the rest of the definition.
        // Read in the image offsets.
        file >> temp.offsetX >> temp.offsetY;
        file >> walkable >> special;
        temp.walkable = (walkable == 'W' ? true : false);

        if (special != '0')
        {
            // We have a special entity.
            if (special == 'S')
            {
                // This is the start position.
                startTile = letter;
            } else if (special == 'C')
            {
                // We gots a treasure chest.
                chestTile = letter;
            } else if (special == 'E')
            {
                // We have an enemy.
                enemyTile = letter;
            }
                
        }

        // add the tile to the tile ID map.
        tileID[letter] = tiles.size();
        tiles.push_back(temp);
    }

    // Allocate space for the tiles in the map data structure.
    map.numTiles = tiles.size();
    map.tiles = new Tile[map.numTiles];
    
    // Copy the tiles into the map's tile structure.
    for (int i = 0; i < map.numTiles; ++i)
    {
        map.tiles[i].offsetX  = tiles[i].offsetX * 32;
        map.tiles[i].offsetY  = tiles[i].offsetY * 32;
        map.tiles[i].walkable = tiles[i].walkable;
    }
    
    //Then we load the map.
    file >> map.width >> map.height;
    
    // Allocate space for the map and the blocked flags.
    map.map = new int[map.height * map.width];
    map.blocked = new bool[map.height * map.width];
    
    int count = 0;
    // Loop through all of the map tile cells.
    while (count < map.width * map.height && !file.eof())
    {
        // Read in the tile ID character.
        file >> letter;
        // Set the tile in the map data structure.
        map.map[count] = tileID[letter];
        // By default all tiles are unblocked.
        map.blocked[count] = false;
        
        // Now handle special tiles.
        if (letter == startTile)
        {
            // This tile is where the player starts.
            startY = count / map.width;
            startX = count - (startY * map.width);
        } else if (letter == chestTile)
        {
            // This tile is a contains a treasure chest.
            int tempY = count / map.width;
            int tempX = count - (tempY * map.width);
            // The last number is how much gold.. We just hard code it to 5 for now.
            Item chest = {tempX, tempY, tiles[tileID['C']].offsetX, tiles[tileID['C']].offsetY, 5};
            items.push_back(chest);
        } else if (letter == enemyTile)
        {
            // This tile contains an enemy.
            int tempY = count / map.width;
            int tempX = count - (tempY * map.width);
            Character enemy = {ENEMY_MAX_HEALTH, tempX, tempY, 64, 0};
            enemies.push_back(enemy);
        }
        ++count;
    }
    
}

// Draw the map. The x and y are used to position the map on the screen.
void drawMap (Map& map, unsigned int x, unsigned int y)
{
    int posx = 128;
    int posy = 0;
    int tile;
    int tileID;
    
    // Loop through all Y-coordinates of tiles in range of the screen.
    for (int yy = y; yy < y+12 && yy < map.height; ++yy)
    {
        // Calculate the index of the first tile on the row (row YY, column 0 - on screen, not overall).
        tile = (yy * map.width) + x;
        
        // Loop through all X-coordinates of tiles in range of the screen.
        for (int xx = x; xx < x+12 && xx < map.width; ++xx)
        {
            // Get the tile ID.
            tileID = map.map[tile];
            // Draw the tile.
            draw(tiles, posx, posy, 32, 32, map.tiles[tileID].offsetX, map.tiles[tileID].offsetY);
            // Advance the position on the screen.
            posx += 32;
            // Move to the next tile.
            ++tile;
        }
        // Advance the on screen position to the next row.
        posy += 32;
        posx = 128;
    }
}

// Draw the enemies.
int drawEnemies (Map& map, std::vector<Character>& goblins, unsigned int x, unsigned int y, unsigned int px, unsigned int py)
{
    int damage = 0;
    // Loop through all enemies.
    for (int i = 0; i < goblins.size(); ++i)
    {
        // If the enemy is in range of the screen...
        if (goblins[i].x >= x && goblins[i].x < x+12 &&
            goblins[i].y >= y && goblins[i].y < y+12)
        {
            // Process the enemy.

            // Unblock their current location.
            map.blocked[goblins[i].x + (goblins[i].y * map.width)] = false;
            
            // If the enemy is in range of the player...
            if (goblins[i].x >= px-1 && goblins[i].x <= px+1 &&
                goblins[i].y >= py-1 && goblins[i].y <= py+1)
            {
                // Enemy in range. Potentially attack.
                if (SDL_GetTicks() - goblins[i].parameter >= 750)
                {
                    // ATTACK!
                    damage++;
                    goblins[i].parameter = SDL_GetTicks();
                }
            } else if (SDL_GetTicks() - goblins[i].parameter >= 1000)
            {
                // Enemy is not in range, but it's ready for an action.
                
                // Move in a random direction.
                switch (std::rand() % 5)
                {
                    case 0:
                        // If the new position is walkable and not blocked, move the enemy.
                        if (!map.blocked[(goblins[i].x + 1) + (goblins[i].y * map.width)] &&
                             map.tiles[map.map[(goblins[i].x + 1) + (goblins[i].y * map.width)]].walkable)
                            goblins[i].x += 1;
                    break; case 1:
                        if (!map.blocked[(goblins[i].x - 1) + (goblins[i].y * map.width)] &&
                             map.tiles[map.map[(goblins[i].x - 1) + (goblins[i].y * map.width)]].walkable)
                            goblins[i].x -= 1;
                    break; case 2:
                        if (!map.blocked[(goblins[i].x) + ((goblins[i].y+1) * map.width)] &&
                             map.tiles[map.map[(goblins[i].x) + ((goblins[i].y+1) * map.width)]].walkable)
                            goblins[i].y += 1;
                    break; case 3:
                        if (!map.blocked[(goblins[i].x) + ((goblins[i].y-1) * map.width)] &&
                             map.tiles[map.map[(goblins[i].x) + ((goblins[i].y-1) * map.width)]].walkable)
                            goblins[i].y -= 1;
                    break; default: break;
                }
                // Reset its action timer.
                goblins[i].parameter = SDL_GetTicks();
            }
            // Set its current position to blocked.
            map.blocked[goblins[i].x + (goblins[i].y * map.width)] = true;

            //Draw the enemy.
            draw(charas, 128 + ((goblins[i].x - x) * 32), (goblins[i].y - y) * 32, 32, 32, 64, 0);
        }
    }
    return damage;
}

// Draw items.
void drawItems (std::vector<Item>& items, unsigned int x, unsigned int y)
{
    // Loop through all items (treasure chests).
    for (int i = 0; i < items.size(); ++i)
    {
        // If they are in range of the screen...
        if (items[i].x >= x && items[i].x < x+12 &&
            items[i].y >= y && items[i].y < y+12)
        {
            // We draw them.
            draw(tiles, 128 + ((items[i].x - x) * 32), (items[i].y - y) * 32, 32, 32, items[i].offsetX * 32, items[i].offsetY * 32);
        }
    }
}

int main (int argc, char* argv[])
{
    // Initialise SDL.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        std::cerr << "Failed to initialise SDL: " << SDL_GetError();
        return 0;
    }
    
    // Set the video mode (ie: initialise the screen for drawing).
    // Set the resolution to 640x480 and the colour depth to 16 bits per pixel (65K Colours)
    // Enable double buffering. To run in fullscreen, uncomment SDL_FULLSCREEN.
    screen = SDL_SetVideoMode(640, 480, 16, /*SDL_FULLSCREEN |*/ SDL_DOUBLEBUF | SDL_HWSURFACE);

    // Set the windows caption.
    SDL_WM_SetCaption("RPG 1: Loading...", NULL);

    srand(SDL_GetTicks());

    // Load the images used for the game.
    font  = loadImage("font.bmp");
    tiles = loadImage("tiles.bmp");
    charas= loadImage("chara.bmp");

    // Keep track of the scrolling of the background.
    Position scroll = {0, 0};

    // The player.
    Character player = {10, 0, 0, 0, 0};
    int gold = 0; // Keep track of the players gold.
    
    // Items (well... treasure chests).
    std::vector<Item> items;
    
    // Enemies.
    std::vector<Character> enemies;
    
    // The map.
    Map map;
    
    // Load the map.
    loadMap(map, player.x, player.y, items, enemies, "map.txt");

    // Set the windows caption.
    SDL_WM_SetCaption("RPG 1  [Press Escape To Quit]", NULL);

    // We use this to limit the amount of input events we allow per second.
    unsigned int lastInput = 0;

    // We use this later to check if keys are pushed down or not.
    Uint8* keys = SDL_GetKeyState(NULL);

    char goldString[8] = {'0',}; // Used to store the string to be printed, done so we dont need to recompute.
    bool attack = false;         // Flag used to force the player to tap the space bar to attack.
    bool gotInput = false;       // Flag used to block all further input until timer has reset.
    bool willHaveInput = false;  // Used to tell the input timer set logic to run.
    bool gameRunning = true;     // Flag used to determine if the game is running or if it should terminate.
    bool gameOver = false;       // If this is set, a game-over screen will appear after the game terminates.
    SDL_Rect healthMeter = {272, 432, 0, 16}; // This rectangle represents the health meter. We don't recompute this if not needed.
    
    // Main game loop.
    while (gameRunning)
    {
        // Update the 'keys' array with new input data.
        SDL_PumpEvents();

        // If 1/4 of a second has passed since the last time we processed input, then we are ready to accept input again.
        if (SDL_GetTicks() - lastInput > 250)
        {
            gotInput = false;
        }
        
        // If we have not already recieved input and the up key is pushed down...
        if (!gotInput && keys[SDLK_UP])
        {
            // The player wants to move up.
            if (map.tiles[map.map[((player.y-1)*map.width)+(player.x)]].walkable
                && !map.blocked[((player.y-1)*map.width)+(player.x)]) player.y -= 1;
            willHaveInput = true;
        }
        if (!gotInput && keys[SDLK_DOWN])
        {
            // The player wants to move down.
            if (map.tiles[map.map[((player.y+1)*map.width)+(player.x)]].walkable
                && !map.blocked[((player.y+1)*map.width)+(player.x)]) player.y += 1;
            willHaveInput = true;
        }
        if (!gotInput && keys[SDLK_RIGHT])
        {
            // The player wants to move right.
            if (map.tiles[map.map[((player.y)*map.width)+(player.x+1)]].walkable
                && !map.blocked[((player.y)*map.width)+(player.x+1)]) player.x += 1;
            willHaveInput = true;
        }
        if (!gotInput && keys[SDLK_LEFT])
        {
            // The player wants to move left.
            if (map.tiles[map.map[((player.y)*map.width)+(player.x-1)]].walkable
                && !map.blocked[((player.y)*map.width)+(player.x-1)]) player.x -= 1;
            willHaveInput = true;
        }
        if (keys[SDLK_SPACE])
        {
            if (!gotInput && !attack)
            {
                attack = true;
                
                // The player wants to attack.
                willHaveInput = true;

                // Check is there anything adjacent to tyhe players position.
                if (map.blocked[((player.y-1)*map.width)+(player.x)] ||
                    map.blocked[((player.y+1)*map.width)+(player.x)] ||
                    map.blocked[((player.y)*map.width)+(player.x-1)] ||
                    map.blocked[((player.y)*map.width)+(player.x+1)] ||
                    map.blocked[((player.y-1)*map.width)+(player.x+1)] ||
                    map.blocked[((player.y+1)*map.width)+(player.x+1)] ||
                    map.blocked[((player.y-1)*map.width)+(player.x-1)] ||
                    map.blocked[((player.y+1)*map.width)+(player.x-1)])
                {
                    // Yes, there is.
                    // Loop through all enemies.
                    for (int i = 0; i < enemies.size(); i++)
                    {
                        // If the enemy is in range of the player...
                        if (enemies[i].x >= player.x-1 && enemies[i].x <= player.x+1 &&
                            enemies[i].y >= player.y-1 && enemies[i].y <= player.y+1)
                        {
                            // Now we know which enemy to attack.
                            // Decrease it's health.
                            enemies[i].health -= 1;
                            // If it's health is zero...
                            if (enemies[i].health <= 0)
                            {
                                // The enemy has died, remove it from the list.
                                // Unblock the current location.
                                map.blocked[((enemies[i].y)*map.width)+(enemies[i].x)] = false;
                                // And delete the enemy from the enemy list.
                                enemies.erase(enemies.begin() + i);
                            }
                            // We can only attack one enemy at a time, so we can stop searching.
                            break;
                        }
                    }
                }
            }
            
            // Check treasure chests, in case the player wanted to get ggold from a chest.
            // Loop through all items.
            for (int i = 0; i < items.size(); ++i)
            {
                // If the item is on the same tile as the enemy...
                if (player.x == items[i].x && player.y == items[i].y)
                {
                    // We found a chest.
                    // Get the gold.
                    gold += items[i].type;
                    // Remove the item from the list.
                    items.erase(items.begin() + i);
                    // Update the gold text.
                    sprintf(goldString, "%d", gold);
                    // there can only be one item on a tile, so we can stop searching.
                    break;
                }
            }
        } else {
            // The player is not pressing the spacebar.
            attack = false;
        }
        
        // We have recieved input, update the timer and scroll the tilemap, if needs be.
        if (willHaveInput)
        {
            gotInput = true;
            willHaveInput = false;
            lastInput = SDL_GetTicks();

            // The player has moved (most likely..), we may need to scroll the map.
            // If the player is near the edge of the screen and the map is not yet fully scrolled, scroll it.
            // C position first. To the left.
            if (player.x - scroll.x < 3 && scroll.x > 0)
            {
                scroll.x--;
                // And to the right.
            } else if (player.x - scroll.x > 8 && scroll.x < map.width-12)
            {
                scroll.x++;
            }
            // Then Y position. Left.
            if (player.y - scroll.y < 3 && scroll.y > 0)
            {
                scroll.y--;
                // And right.
            } else if (player.y - scroll.y > 8 && scroll.y < map.height-12)
            {
                scroll.y++;
            }
        }

        // Clear the screen to black before drawing to it.
        SDL_FillRect(screen, NULL, 0);
        
        // Draw the background map.
        drawMap(map, scroll.x, scroll.y);
        
        // Draw all the items.
        drawItems(items, scroll.x, scroll.y);
        
        // Draw all the enemies.
        player.health -= drawEnemies(map, enemies, scroll.x, scroll.y, player.x, player.y);
        // Player death.
        if (player.health <= 0)
        {
            gameOver = true;
            gameRunning = false;
            drawText(304, 432, 200, "Game Over!");
        }

        // Draw the player.
        draw(charas, 128 + ((player.x-scroll.x)*32), (player.y-scroll.y)*32, 32, 32, player.image*32, 0);

        // Draw the players gold count.
        draw(tiles, 144, 416, 32, 32, 32, 64);
        drawText(184, 416, 100, goldString);
        
        // Draw the health meter.
        healthMeter.w = player.health * 24;
        SDL_FillRect(screen, &healthMeter, SDL_MapRGB(screen->format, 0, 0, 255));
        
        // Loop through all enemies...
        for (int i = 0; i < enemies.size(); i++)
        {
            // and check are they in attach range of the player...
            if (enemies[i].x >= player.x-1 && enemies[i].x <= player.x+1 &&
                enemies[i].y >= player.y-1 && enemies[i].y <= player.y+1)
            {
                // If so, we are beside an enemy, so we want to draw it's health meter.
                // Create a rectangle to represent the enemies health meter.
                SDL_Rect enemyMeter = {272, 416, enemies[i].health * (240 / ENEMY_MAX_HEALTH), 8};
                // And draw it.
                SDL_FillRect(screen, &enemyMeter, SDL_MapRGB(screen->format, 255, 0, 0));
                // We can only draw one enemies health meter at a time, so we can stop checking.
                break;
            }
        }

        // Switch back buffer and screen - ie: display what we've just drawn.
        SDL_Flip(screen);

        // If escape was pressed, we bail out.
        if (keys[SDLK_ESCAPE]) gameRunning = false;
    }

    // If the player died, we want to pause on the game over screen until the player presses escape.
    while (gameOver)
    {
        // Check for new input.
        SDL_PumpEvents();
        // check is escape being pressed.
        if (keys[SDLK_ESCAPE]) gameOver = false;
    }

    // Unload the map.
    delete [] map.tiles;
    delete [] map.map;
    delete [] map.blocked;

    // Unload the bitmaps.
    SDL_FreeSurface(font);
    SDL_FreeSurface(tiles);
    SDL_FreeSurface(charas);
    // We do not free the screen, SDL does this itself when we call SDL_Quit().

    // Let SDL clean itself up.
    SDL_Quit();

}
