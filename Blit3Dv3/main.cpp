/*
	Asteroids ship movement example
*/
//memory leak detection
#define CRTDBG_MAP_ALLOC
#ifdef _DEBUG
	#ifndef DBG_NEW
		#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
		#define new DBG_NEW
	#endif
#endif  // _DEBUG

#include <stdlib.h>
#include <crtdbg.h>

#include "Blit3D.h"

#include "Ship.h"
#include<random>
#include <fstream>
#include <iostream>
#include<algorithm>

Blit3D *blit3D = NULL;

//GLOBAL DATA
extern std::mt19937 rng;

double elapsedTime = 0;
float timeSlice = 1.f / 120.f;

Sprite *backgroundSprite = NULL; //a pointer to a background sprite
Ship *ship = NULL;

bool shoot = false; //should we try to shoot?
std::vector<Shot> shotList;

std::vector<Asteroid> asteroidList;

Sprite* largeAsteroid = NULL;
Sprite* mediumAsteroid = NULL;
Sprite* smallAsteroid = NULL;

std::uniform_real_distribution<float> spinDist2(-100.f, 100.f);
std::uniform_real_distribution<float> mediumSpeedDist2(200.f, 500.f);
std::uniform_real_distribution<float> smallSpeedDist2(400.f, 850.f);
std::uniform_real_distribution<float> angleDist2(glm::radians(5.f), glm::radians(30.f));
std::uniform_real_distribution<float> angleDist3(glm::radians(35.f), glm::radians(60.f));
std::uniform_real_distribution<float> directionDist2(0.f, 360.f);

std::vector<Sprite*> explosionSpriteList;
std::vector<Explosion> explosionList;
int level = 0;
enum class GameState {PLAYING, PAUSE, GAMEOVER, START, TITLESCREEN, HIGHSCORE, HIGHSCOREENTRY};
GameState gameState = GameState::TITLESCREEN;

AngelcodeFont* OriginTech90font = NULL;
AngelcodeFont* RobotCrush100 = NULL;

Sprite* lifeSprite = NULL;
Sprite* titleGraphic = NULL;

int score = 0;
std::string initials = "XXX";
int currentLetter = 0;

class HiScore
{
public:
	std::string initials;
	int score;
};

std::vector<HiScore> hiScoreTable;

//comparison function, required by std::sort
bool sortHiScores(HiScore A, HiScore B)
{
	return (A.score > B.score);
}

void SortHisScores()
{
	std::sort(hiScoreTable.begin(), hiScoreTable.end(), sortHiScores);
}

void SaveHiScoreTable()
{
	//filestream for output
	std::ofstream hiScorefile;

	//open up a file for writing
	hiScorefile.open("HiScores.dat");

	if (!hiScorefile.is_open())
	{
		std::cout << " Can't open HiScores.dat for writing!" << std::endl;
		exit(-1);
	}

	for (auto hs : hiScoreTable)
	{
		hiScorefile << hs.initials << std::endl;
		hiScorefile << hs.score << std::endl;
	}
	hiScorefile.close();
}

void LoadHiScoreTable()
{
	hiScoreTable.clear();
	//filestream for input
	std::ifstream hiScorefile;

	//open up a file for reading
	hiScorefile.open("HiScores.dat");

	//error check, make sure we found the file and opened it
	if (!hiScorefile.is_open())
	{
		std::cout << " Can't open HiScores.dat!" << std::endl;
		exit(-1);
	}

	for (int i = 0; i < 5; ++i)
	{
		HiScore hs;
		getline(hiScorefile, hs.initials);
		std::string temp;
		getline(hiScorefile, temp);
		hs.score = std::stoi(temp);
		hiScoreTable.push_back(hs);
	}

	hiScorefile.close();

	SortHisScores();
}

//returns true if the current score belongs on the high score table
bool CheckForHiScore()
{
	SortHisScores();	
	if (score <= hiScoreTable[hiScoreTable.size() - 1].score) return false;	
	return true;
}

void InsertScore()
{
	HiScore hs;
	hs.initials = initials;
	hs.score = score;

	hiScoreTable.push_back(hs);
	SortHisScores();

	//remove lowest score
	hiScoreTable.pop_back();
}

void MakeLevel()
{
	level++;

	//turn on invulnerability - activate the player's shield
	ship->shieldTimer = 3.f;

	//move the ship back to the center of the screen
	ship->position = glm::vec2(1920.f / 2, 1080.f / 2);
	ship->angle = 90;
	ship->velocity = glm::vec2(0.f, 0.f);

	//cleanup old shots, asteroids
	shotList.clear();
	asteroidList.clear();
	explosionList.clear();

	for (int i = 0; i < level; ++i)
	{
		//asteroidList.push_back(AsteroidFactory(MakeRandomAsteroidSize()));
		asteroidList.push_back(AsteroidFactory(AsteroidSize::LARGE));
	}
}

void Init()
{
	//seed our RNGs
	InitializeRNG();

	//turn cursor off
	blit3D->ShowCursor(false);

	//load a font
	OriginTech90font = blit3D->MakeAngelcodeFontFromBinary32("Media\\tech.bin");
	RobotCrush100 = blit3D->MakeAngelcodeFontFromBinary32("Media\\RobotCrush100.bin");

	//load our small ship life counter
	lifeSprite = blit3D->MakeSprite(0,0,33,37,"Media\\life.png");

	//load our background image: the arguments are upper-left corner x, y, width to copy, height to copy, and file name.
	backgroundSprite = blit3D->MakeSprite(0, 0, 1920, 1080, "Media\\background.png");

	//create a ship
	ship = new Ship;
	//load a sprite off of a spritesheet
	for (int i = 0; i < 4; ++i)
		ship->spriteList.push_back(blit3D->MakeSprite(i * 66, 0, 66, 94, "Media\\fourship.png"));

	ship->position = glm::vec2(1920.f / 2, 1080.f / 2 );

	//load the shot graphic
	ship->shotSprite = blit3D->MakeSprite(0, 0, 8, 8, "Media\\shot.png");
	//load the shield graphic
	ship->shieldSprite = blit3D->MakeSprite(0, 0, 87, 87, "Media\\shield2.png");
	
	//set the clear colour
	glClearColor(1.0f, 0.0f, 1.0f, 0.0f);	//clear colour: r,g,b,a 	

	//load our asteroid graphics
	largeAsteroid = blit3D->MakeSprite(0, 0, 269, 261, "Media\\big.png");
	mediumAsteroid = blit3D->MakeSprite(0, 0, 108, 114, "Media\\medium.png");
	smallAsteroid = blit3D->MakeSprite(0, 0, 56, 56, "Media\\small.png");

	//load explosion graphics
	for (int i = 0; i < 7; ++i)
	{
		explosionSpriteList.push_back(blit3D->MakeSprite(91 + i * 34, 9, 33, 33, "Media\\M484ExplosionSet1.png"));
	}

	titleGraphic = blit3D->MakeSprite(0, 0, 1920, 1080, "Media\\page.png");

	LoadHiScoreTable();
}

void DeInit(void)
{
	if(ship != NULL) delete ship;
	//any sprites/fonts still allocated are freed automatically by the Blit3D object when we destroy it
}

void Update(double seconds)
{
	//only update time to a maximun amount - prevents big jumps in 
	//the simulation if the computer "hiccups"
	if (seconds < 0.15)
		elapsedTime += seconds;
	else elapsedTime += 0.15;

	switch (gameState)
	{
	case GameState::PLAYING:
		//update by a full timeslice when it's time
		while (elapsedTime >= timeSlice)
		{
			elapsedTime -= timeSlice;
			ship->Update(timeSlice);

			if (shoot) ship->Shoot(shotList);

			//iterate backwards through the shotlist,
			//so we can erase shots without messing up the vector
			//for the next loop
			for (int i = shotList.size() - 1; i >= 0; --i)
			{
				//shot Update() returns false when the bullet should be killed off
				if (!shotList[i].Update(timeSlice))
					shotList.erase(shotList.begin() + i);
			}

			//same for explosions, iterate backqwards through teh vector so we can safely remove some
			for (int i = explosionList.size() - 1; i >= 0; --i)
			{
				if (!explosionList[i].Update(timeSlice))
				{
					explosionList.erase(explosionList.begin() + i);
				}
			}

			for (auto& A : asteroidList) A.Update(timeSlice);

			//collision check between shots and asteroids
			for (int aIndex = asteroidList.size() - 1; aIndex >= 0; --aIndex)
			{
				for (int sIndex = shotList.size() - 1; sIndex >= 0; --sIndex)
				{
					//check for collision
					if (Collide(shotList[sIndex], asteroidList[aIndex]))
					{
						//there was a collision!
						//remove the shot
						shotList.erase(shotList.begin() + sIndex);

						//add to our score
						score += 10;

						//handle the asteroid split
						switch (asteroidList[aIndex].size)
						{
						case AsteroidSize::LARGE:
						{
							//make an explosion here
							Explosion e(asteroidList[aIndex].position, 6.f);
							explosionList.push_back(e);
							//make a couple of medium asteroids
							//calculate the original asteroid's angle of motion
							float angle = atan2f(asteroidList[aIndex].velocity.y, asteroidList[aIndex].velocity.x);

							for (int i = 0; i < 2; ++i)
							{
								float angleThisAsteroid = angle;
								Asteroid A;
								A.size = AsteroidSize::MEDIUM;
								A.position = asteroidList[aIndex].position;

								A.spin = spinDist2(rng);
								A.radius2 = (81.f / 2) * (81.f / 2);
								A.sprite = mediumAsteroid;
								A.angle = directionDist2(rng);

								//perturb the velocity angle
								if (i == 0) angleThisAsteroid -= angleDist2(rng);
								else angleThisAsteroid += angleDist2(rng);


								//create a velocity from this angle
								A.velocity.x = cos(angleThisAsteroid);
								A.velocity.y = sin(angleThisAsteroid);

								A.velocity *= mediumSpeedDist2(rng);

								asteroidList.push_back(A);
							}
						}
						break;
						case AsteroidSize::MEDIUM:
						{
							//make an explosion here
							Explosion e(asteroidList[aIndex].position, 2.f);
							explosionList.push_back(e);
							//make a couple of small asteroids


							float angle = atan2f(asteroidList[aIndex].velocity.y, asteroidList[aIndex].velocity.x);

							for (int i = 0; i < 2; ++i)
							{
								float angleThisAsteroid = angle;
								Asteroid B;
								B.size = AsteroidSize::SMALL;
								B.position = asteroidList[aIndex].position;

								B.spin = spinDist2(rng);
								B.radius2 = (50.f / 2) * (50.f / 2);
								B.sprite = smallAsteroid;
								B.angle = directionDist2(rng);

								//perturb the velocity angle
								if (i == 0) angleThisAsteroid -= angleDist2(rng);
								else angleThisAsteroid += angleDist2(rng);


								//create a velocity from this angle
								B.velocity.x = cos(angleThisAsteroid);
								B.velocity.y = sin(angleThisAsteroid);

								B.velocity *= mediumSpeedDist2(rng);

								asteroidList.push_back(B);
							}
						}
						break;
						case AsteroidSize::SMALL: //small asteroids
						{
							//make an explosion here
							Explosion e(asteroidList[aIndex].position, 1.f);
							explosionList.push_back(e);
						}
						break;
						}//end switch
						asteroidList.erase(asteroidList.begin() + aIndex);

						//advance to next asteroid
						break;
					}
				}
			}// end of collision loop, asteroids vsshots

			if (asteroidList.empty())
			{
				MakeLevel();
				//might not want to pause, can comment this out
				gameState = GameState::PAUSE;
				break;
			}

			for (auto A : asteroidList)
			{
				//check for collision with the ship
				if (Collide(ship, A))
				{
					//take away a life
					ship->lives--;
					//check for game over
					if (ship->lives < 1)
					{
						gameState = GameState::GAMEOVER;
						break;
					}

					//turn on invulnerability - activate the player's shield
					ship->shieldTimer = 3.f;	

					//make an explosion
					Explosion e(ship->position, 3.f);
					explosionList.push_back(e);
				}
			}
		}
		break; //end PLAYING state

		case GameState::START:
			level = 0;
			ship->lives = 3;
			score = 0;
			currentLetter = 0;
			MakeLevel();
			gameState = GameState::PAUSE;
			break;

		case GameState::HIGHSCOREENTRY:

			break;

		case GameState::PAUSE:
		case GameState::GAMEOVER:
		case GameState::TITLESCREEN:
		case GameState::HIGHSCORE:
		default:
			break;
		
	}//end gameState switch
}

void Draw(void)
{
	// wipe the drawing surface clear
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//draw stuff here

	switch (gameState)
	{
	case GameState::HIGHSCOREENTRY:
	{
		backgroundSprite->Blit(1920.f / 2, 1080.f / 2);
		std::string titleString = "HIGH SCORE TABLE";
		float widthTitle = OriginTech90font->WidthText(titleString);
		OriginTech90font->BlitText((1920.f / 2) - (widthTitle /2 ), 1080.f - (100 + 75), titleString);

		OriginTech90font->BlitText(400, 590, "ENTER YOUR INITIALS: ");
		
		for(int i = 0; i <= currentLetter; ++i)
			OriginTech90font->BlitText(900 + i * 50, 400 , &initials[i]);

		//draw the score
		std::string scoreString = std::to_string(score);
		OriginTech90font->BlitText(1000, 1080 - 5, scoreString);
	}
		break;

	case GameState::HIGHSCORE:
	{
		backgroundSprite->Blit(1920.f / 2, 1080.f / 2);
		std::string titleString = "HIGH SCORE TABLE";
		float widthTitle = OriginTech90font->WidthText(titleString);
		OriginTech90font->BlitText((1920.f / 2) - (widthTitle / 2), 1080.f - (100 + 75), titleString);
		//draw high scores
		for (int i = 0; i < hiScoreTable.size(); ++i)
		{
			std::string scoreLine = hiScoreTable[i].initials + " " + std::to_string(hiScoreTable[i].score);
			OriginTech90font->BlitText(500, 1080.f - (300 + 120 * i), scoreLine);
		}

		//draw the score
		std::string scoreString = std::to_string(score);
		OriginTech90font->BlitText(1000, 1080 - 5, scoreString);
	}
		break;
	case GameState::TITLESCREEN:
	{
		titleGraphic->Blit(1920.f / 2, 1080.f / 2);
		std::string titleString = "ASTROBLASTER";
		float widthTitle = RobotCrush100->WidthText(titleString);
		RobotCrush100->BlitText(200.f, 1080.f / 2, titleString);
	}
		break;
	case GameState::PAUSE:
	{
		backgroundSprite->Blit(1920.f / 2, 1080.f / 2);

		for (auto& A : asteroidList) A.Draw();

		//draw the ship
		ship->Draw();

		//draw the shots
		for (auto& S : shotList) S.Draw();

		//draw explosions
		for (auto& E : explosionList) E.Draw();

		//draw our life HUD
		for (int i = 0; i < ship->lives; ++i)
		{
			lifeSprite->Blit(100 + i * 32, 1080 - 50);
		}

		std::string paused = "PAUSED - press any key to continue";
		float widthText = OriginTech90font->WidthText(paused);
		OriginTech90font->BlitText(1920.f / 2.f - widthText / 2.f, 1080.f / 2.f, paused);
		//draw the score
		std::string scoreString = std::to_string(score);
		OriginTech90font->BlitText(1000, 1080 - 5, scoreString);
	}
		break;

	case GameState::GAMEOVER:
	{
		backgroundSprite->Blit(1920.f / 2, 1080.f / 2);

		for (auto& A : asteroidList) A.Draw();

		//draw the ship
		ship->Draw();

		//draw the shots
		for (auto& S : shotList) S.Draw();

		//draw explosions
		for (auto& E : explosionList) E.Draw();

		//draw our life HUD
		for (int i = 0; i < ship->lives; ++i)
		{
			lifeSprite->Blit(100 + i * 32, 1080 - 50);
		}

		std::string gameOver = "GAME OVER";
		float widthText = OriginTech90font->WidthText(gameOver);
		OriginTech90font->BlitText(1920.f / 2.f - widthText / 2.f, 1080.f / 2.f, gameOver);
		std::string gameOver2 = "Press G to start a new game";
		float widthText2 = OriginTech90font->WidthText(gameOver2);
		OriginTech90font->BlitText(1920.f / 2.f - widthText2 / 2.f, 1080.f / 2.f - 100.f, gameOver2);
		//draw the score
		std::string scoreString = std::to_string(score);
		OriginTech90font->BlitText(1000, 1080 - 5, scoreString);
	}
	break;
	case GameState::START:	
	case GameState::PLAYING:
		//draw the background in the middle of the screen
		//the arguments to Blit(0 are the x, y pixel coords to draw the center of the sprite at, 
		//starting as 0,0 in the bottom-left corner.
		backgroundSprite->Blit(1920.f / 2, 1080.f / 2);

		for (auto& A : asteroidList) A.Draw();

		//draw the ship
		ship->Draw();

		//draw the shots
		for (auto& S : shotList) S.Draw();

		//draw explosions
		for (auto& E : explosionList) E.Draw();

		//draw our life HUD
		for (int i = 0; i < ship->lives; ++i)
		{
			lifeSprite->Blit(100 + i * 32, 1080 - 50);
		}

		//draw the score
		std::string scoreString = std::to_string(score);
		OriginTech90font->BlitText(1750, 1080 - 25, scoreString);
		break;	
	}
}

//the key codes/actions/mods for DoInput are from GLFW: check its documentation for their values
void DoInput(int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		blit3D->Quit(); //start the shutdown sequence

	switch (gameState)
	{
	case GameState::HIGHSCOREENTRY:
		if (key == GLFW_KEY_ENTER && action == GLFW_RELEASE)
		{
			InsertScore();
			gameState = GameState::HIGHSCORE;
		}
		else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z && action == GLFW_RELEASE)
		{
			initials[currentLetter] = std::toupper(key);
			currentLetter++;
			if (currentLetter > 2)
			{
				//all three initials now entered
				InsertScore();
				gameState = GameState::HIGHSCORE;
				currentLetter = 0;
			}
		}

		break;
	case GameState::HIGHSCORE:
		if (action == GLFW_RELEASE)
			gameState = GameState::TITLESCREEN;
		break;
	case GameState::TITLESCREEN:
		if (action == GLFW_RELEASE)
			gameState = GameState::START;
		break;	
	case GameState::GAMEOVER:
		if (key == GLFW_KEY_G && action == GLFW_RELEASE)
			if (CheckForHiScore())
			{
				gameState = GameState::HIGHSCOREENTRY;
			}
			else gameState = GameState::HIGHSCORE;
		break;
	case GameState::PAUSE:
		if (action == GLFW_RELEASE)
			gameState = GameState::PLAYING;
		break;
	case GameState::PLAYING:
		if (key == GLFW_KEY_A && action == GLFW_PRESS)
			ship->turningLeft = true;

		if (key == GLFW_KEY_A && action == GLFW_RELEASE)
			ship->turningLeft = false;

		if (key == GLFW_KEY_D && action == GLFW_PRESS)
			ship->turningRight = true;

		if (key == GLFW_KEY_D && action == GLFW_RELEASE)
			ship->turningRight = false;

		if (key == GLFW_KEY_W && action == GLFW_PRESS)
			ship->thrusting = true;

		if (key == GLFW_KEY_W && action == GLFW_RELEASE)
			ship->thrusting = false;

		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
			shoot = true;

		if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
			shoot = false;

		if (key == GLFW_KEY_P && action == GLFW_RELEASE)
			gameState = GameState::PAUSE;
		break;

	case GameState::START:
	default:
		break;
	}//end gameState switch
}

int main(int argc, char *argv[])
{
	//memory leak detection
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	//set X to the memory allocation number in order to force a break on the allocation:
	//useful for debugging memory leaks, as long as your memory allocations are deterministic.
	//_crtBreakAlloc = X;

	blit3D = new Blit3D(Blit3DWindowModel::BORDERLESSFULLSCREEN_1080P, 640, 400);

	//set our callback funcs
	blit3D->SetInit(Init);
	blit3D->SetDeInit(DeInit);
	blit3D->SetUpdate(Update);
	blit3D->SetDraw(Draw);
	blit3D->SetDoInput(DoInput);

	//Run() blocks until the window is closed
	blit3D->Run(Blit3DThreadModel::SINGLETHREADED);
	SaveHiScoreTable();
	if (blit3D) delete blit3D;
}