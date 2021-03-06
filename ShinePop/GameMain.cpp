#include "GameMain.h"

/*
 *   [0,0]  [1,0],  [2,0] ...  [BOARD_WIDTH-1, 0]
 *   [0,1]  [1,1],  [2,1] ...  [BOARD_WIDTH-1, 1]
 *                        ...
 *   [0,BOARD_HEIGHT-1]  [1,BOARD_HEIGHT-1],  [2,BOARD_HEIGHT-1] ...  [BOARD_WIDTH-1, BOARD_HEIGHT-1]
 *
 */


const char GameMain::tx[MAX_COLOR_COUNT][3] = {"◆","▲","●","■","★","╋","▼","△","◇","◎"}; 
const byte GameMain::colors[] = { COLOR_LIGHT_GREEN, COLOR_LIGHT_CYAN, COLOR_LIGHT_RED, COLOR_LIGHT_MAGENTA,  COLOR_LIGHT_YELLOW, COLOR_CYAN, COLOR_RED,COLOR_WHITE,  COLOR_LIGHT_BLUE,COLOR_BLUE}; 


#ifdef _DEBUG
uchar testGameMain[7][9] = {
	{7, 2, 4, 7, 2, 3, 2, 2, 1},
	{7, 3, 4, 1, 2, 4, 2, 1, 2},
	{3, 2, 2, 1, 1, 4, 1, 3, 3},
	{4, 4, 2, 4, 4, 2, 6, 6, 4},
	{5, 1, 4, 6, 6, 4, 4, 6, 4},
	{6, 2, 4, 1, 2, 3, 4, 4, 6},
	{1, 2, 3, 1, 2, 3, 1, 2, 3},
};
#endif

GameMain::~GameMain()
{
}

/*
 * ゲーム画面
 */
GameMain::GameMain()
{
	om = om->Instance ();
	im = im->Instance ();
	tm = tm->Instance ();
	gm = gm->Instance ();

	InitialNewGame();
}

void GameMain::GenerateNewGems ()
{
	COORD NO_SWAPABLE_GEM = { -1, -1 };
	
	int i = 0, j = 0;
	for (i = 0; i < BOARD_WIDTH; i++) {
		for (j = 0; j < BOARD_HEIGHT; j++) {
#ifdef _DEBUG
			GEM gem = { testGameMain[i][j], GEM_TYPE_NORMAL, 1, 1, false, true, 0, 0};
#else
			GEM gem = { GetRandGemID(), GEM_TYPE_NORMAL, 1, 1, true, true, 0, 0};
#endif
			_gems[i][j] = gem;
		}
	}
	// ProcessFalldown ();
	while ( true ){
		// すべての行と列を処理
		chgRows = ( 1 << BOARD_HEIGHT ) - 1;
		chgCols = ( 1 << BOARD_WIDTH ) - 1;
		ProcessAllMatch ( );
		for (i = 0; i < BOARD_WIDTH; i++) {
			for (j = 0; j < BOARD_HEIGHT; j++) {
				_gems[i][j].s = GEM_TYPE_NORMAL;
			}
		}
		// 生成したブロックが消せるかどうかをチェック
		COORD hint = FindMostSwapableGem ();
		if ( hint.X > 0 && hint.Y > 0 ) break;
		else {
			i = rand () % BOARD_WIDTH;
			for (j = 0; j < BOARD_HEIGHT; j++) {
				_gems[i][j].del = true;
			}
			ProcessFalldown ( false );
		}
	}
}


/*
 * ゲーム画面に入る前の初期化
 */
void GameMain::InitialNewGame()
{
	// initial rand seed
#ifdef _DEBUG
	srand ( 1 );
#else
	srand( (unsigned int)time(NULL) );
#endif

	// UIの初期化設定
	curPos.X = BOARD_WIDTH / 2;
	curPos.Y = BOARD_HEIGHT / 2;
	selected = false;

	for ( int k = 0; k < BOARD_GEM_COUNT; k++ ) { 
		gemProbTable[k] = k+1;
		gemStat[k] = 0;
	}

	// タイマー初期化
	ChangeFeverTimeState( false );

	// コンボ初期化
	comboCount = 0;
	comboLog = std::queue<unsigned long>();
	comboGaugeValue = 0;
	lastComboGaugeChanged = 0;

	// ブロックを初期化
	GenerateNewGems ();

	// 各種ブロックの個数を集計
	TotalUpGems();

	// flags初期化
	chgCols = 0;
	chgRows = 0;
	animCols = 0;
	hintPos.X = -1; hintPos.Y = -1;
	lastDelete = 0;

	// 最後のスワップの時間
	lastSwap = 0;

	// スコア初期化
	gm->score = 0;
	SetScore(0);
	scoreGaugeValue = 0;
}

/*
 * 各種ブロックの個数を集計する
 */
void GameMain::TotalUpGems ()
{
	// pThreshold は > 0 の小数
	// pThresboldが小さいければ小さいほどバランスのよいIDが作られる（はず）
	const float pThreshold = 1.5f;

	// 初期化
	for ( int k = 0; k< BOARD_GEM_COUNT; k++ ) { gemStat[k] = 0; }

	// 集計
	int total = 0;
	for ( int i = 0; i < BOARD_WIDTH; i++ ) {
		for ( int j = 0; j < BOARD_HEIGHT; j++ ) {
			if ( _gems[i][j].s == GEM_TYPE_NORMAL ) {
				gemStat[_gems[i][j].id]++;
				total++; 
			}
		}
	}

	// 確率テーブルを生成
	int cThreshold = static_cast<int>( std::floor ( total / BOARD_GEM_COUNT * pThreshold ) + 1 );
	for ( int k = 0; k < BOARD_GEM_COUNT; k++ ) { 
		gemProbTable[k] = max ( cThreshold - gemStat[k], 0 );
	}
	for ( int k = 1; k < BOARD_GEM_COUNT; k++ ) { 
		gemProbTable[k] += gemProbTable[k-1];
	}
}

void GameMain::Start ()
{
	// Game main loop
	bool exit = false;
	bool isTimeUp = false;
	bool isReadyGoFinished = false;
	gm->ResetFrameCounter ();
	int fc;
	do {
		fc = gm->GetFrameCount ();
		gm->BeginFrame ();
		// 背景を初期化
		if ( fc == 0 ) {
			Redraw ();
		// READY GO 画面
		} else if ( fc < READY_DURATION + GO_DURATION ) {
			DrawReadyGo ();
		// READY GO が終わったらタイマー系を初期化
		} else if ( fc >= READY_DURATION + GO_DURATION && !isReadyGoFinished ) {
			tm->Start();
			om->ClearScreen();
			ChangeFeverTimeState ( false );
			isReadyGoFinished = true;
			Redraw ( true );
		// ゲームのメイン画面 ・　本物のゲームループ
		} else {
			exit = !ProcessGameInputEvent();
			if ( lastDelete == 0 && animCols == 0 && (chgCols != 0 || chgRows != 0) ) {
				ProcessMatchLines();
			}
			ProcessAnim();

			Redraw();
			if ( Timer() ){
				isTimeUp = true;
				break;
			}
		}
		gm->EndFrame ();
	}while( !exit );

	if ( isTimeUp ) gm->ChangeGameState ( GAME_STATE_RESULT );
	else gm->ChangeGameState ( GAME_STATE_TITLE );
}


/*
 * ゲームメイン画面のタイマー
 *
 * @return 時間がなくなったらtrue
 */
bool GameMain::Timer()
{
	// スコアのアニメを処理
	// logNのスピードで増加（バラつきが大きいスコア増分にも対応できるように）
	if ( gm->score > scoreGaugeValue ) {
		scoreGaugeValue +=  ( gm->score - scoreGaugeValue ) / 40 + 20 ;
		if ( gm->score < scoreGaugeValue ) {
			scoreGaugeValue = gm->score;
		}
		DrawScore( scoreGaugeValue );
	}


	if ( tm->IsPause() ) return false;

	curTick = tm->CurrentTick();
	
	float counter = 1 - (float)curTick / ( GAME_TIME ) ;

	// コンボゲージの減少
	if ( lastComboGaugeChanged + COMBO_THRESHOLD_TIME < curTick && comboGaugeValue > 0) {
		comboGaugeValue--;
		lastComboGaugeChanged = curTick;
	}
	
	// コンボのチェック
	if ( comboLog.size() > 0 ) {
		if ( curTick - comboLog.back() > COMBO_THRESHOLD_TIME ) { // コンボ切れたら
			// 履歴をリセット
			comboLog = std::queue<unsigned long>();
			comboCount = 0;
		}
	}
	DrawComboGauge();

	// ヒント
	if ( hintPos.X == -1 && hintPos.Y == -1 && curTick - lastSwap > HINT_THRESHOLD_TIME ) {
		hintPos = FindMostSwapableGem ();
		if ( hintPos.X >= 0 && hintPos.Y >= 0 ) {
			_gems[hintPos.X][hintPos.Y].redraw = true;
		}
		// 前の計算に誤りがなければ対応いらない
	}

	if ( counter > 0 ) {
		// タイマーを表示
		om->Print( COLOR_GREEN, LEFT_MARGIN, TOP_MARGIN + BOARD_HEIGHT * 3 + 1, "TIME: ");
		
		int len = BOARD_WIDTH*6 - 5;
		int cur = (int)floor( counter * len );

		// TODO 改善可能
		for(int i=1; i < len; i++ ){
			om->Print( cur >= i ?  ( counter <= GAME_TIME_WARNING_LINE ? COLOR_RED : COLOR_LIGHT_GREEN ) : COLOR_LIGHT_BLUE, "|");
		}

		// フィーバータイマーを表示
		if ( isFeverMode && feverEndTime <= curTick ) {
			ChangeFeverTimeState(false);
		}
		if ( isFeverMode ) {
			float feverCounter = (float)( feverEndTime - curTick ) / 1000.0f;
			om->SetColor ( feverCounter <= FEVER_TIME_WARNING_LINE ? COLOR_RED : COLOR_LIGHT_GREEN  );
			om->GotoXY(LEFT_MARGIN + BOARD_WIDTH * 3 - sizeof("[ FEVERTIME: 12345 ]") / 2, TOP_MARGIN - 1);
			om->Print("[ FEVERTIME: %5.2f ]", feverCounter );
		}

		return false;
	} else {
		return true;
	}
}



/*
 * コンボゲージとコンボ数を描画する
 */
void GameMain::DrawComboGauge () {
	// コンボ数
	if ( comboCount > 0 ) {
		om->Print( COLOR_GREEN, LEFT_MARGIN, TOP_MARGIN - 3, "COMBO " );
		om->Print( COLOR_LIGHT_GREEN, "%4.d", comboCount );
	} else {
		om->Print( COLOR_CLEAR, LEFT_MARGIN, TOP_MARGIN - 3, "           " );
	}
	
	// コンボゲージ
	om->Print( COLOR_GREEN, LEFT_MARGIN + BOARD_WIDTH*3 - 2, TOP_MARGIN - 3, "COMBO GAUGE: ");
	int len = BOARD_WIDTH * 3 - sizeof("COMBO GAUGE: ") + 4;
	int cur = (int)floor( ( (float)comboGaugeValue / COMBO_GAUGE_LEN ) * len );
	//  TODO 要改善
	for ( int i=1; i<len; i++ ){
		om->Print (  cur >= i ? COLOR_LIGHT_GREEN : COLOR_LIGHT_BLUE, "|" );
	}
}

/*
 * フィーバータイム開始・停止
 */
void GameMain::ChangeFeverTimeState(bool state)
{
	isFeverMode = state;

	if ( isFeverMode ) {
		om->SetColor( COLOR_LIGHT_GREEN );
		feverEndTime = tm->CurrentTick() + FEVER_TIME_DURATION ;
	} else {
		om->SetColor( COLOR_RED );
		// TODO 仕様要確認
		// フィーバータイムが終わる時点に交換履歴をクリアするかどうか
		// swapLog = std::queue<SWAP_LOG>();
	}

	
	// フィーバー画面の枠を描く
	for ( int i = 0; i < BOARD_WIDTH * 3; i++ ) {
		om->Print( LEFT_MARGIN+i*2, TOP_MARGIN - 1, "─" );
		om->Print( LEFT_MARGIN+i*2, TOP_MARGIN + BOARD_HEIGHT * 3, "─");
	}
	for ( int j = 0; j < BOARD_HEIGHT * 3; j++ ) {
		om->Print ( LEFT_MARGIN - 2, TOP_MARGIN + j, "│" );
		om->Print ( LEFT_MARGIN + BOARD_WIDTH * 6, TOP_MARGIN + j, "│" );
	}

	om->Print ( LEFT_MARGIN - 2, TOP_MARGIN - 1, "┌" );
	om->Print ( LEFT_MARGIN - 2, TOP_MARGIN + BOARD_HEIGHT * 3, "└" );
	om->Print ( LEFT_MARGIN + BOARD_WIDTH * 6, TOP_MARGIN - 1, "┐" );
	om->Print ( LEFT_MARGIN + BOARD_WIDTH * 6, TOP_MARGIN + BOARD_HEIGHT * 3, "┘" );

}


/*
 * スコアを増やす
 */
void GameMain::IncScore(int s) { SetScore( gm->score + s); }

/*
 * スコアを変更
 */
void GameMain::SetScore(int s) { gm->score = s; }

/*
 * スコアを描く（アニメ用）
 */
void GameMain::DrawScore ( int s )
{
	om->Print( COLOR_GREEN, LEFT_MARGIN, TOP_MARGIN + BOARD_HEIGHT * 3 + 3, "SCORE:   ");
	om->Print( COLOR_LIGHT_GREEN, "%10d", s );
}

/*
 * 入力の処理
 * 
 * @return true for process successfully or false for exit
 */
bool GameMain::ProcessGameInputEvent()
{
	INPUT_EVENT ie = im->GetInputEvent();
	COORD oldPos = curPos;

	if( ( ie.keyPressed || ie.mouseClicked ) && !tm->IsPause() ) {
		// 今の位置を描き直す
		_gems[curPos.X][curPos.Y].redraw = true;

		bool isProcessMatch = false; // マッチ処理するか

		// キーやマウスの処理
		if ( ie.keyPressed ) {
			switch ( ie.key )
			{
				case KEY_RIGHT:
					if (curPos.X < BOARD_WIDTH -1) {
						if (!selected) { curPos.X++; }
						else if ( Swap(curPos, SWAP_DIRECTION_RIGHT) > 0) {
							curPos.X++; isProcessMatch = true;
						}
					}
					break;
				case KEY_LEFT:
					if (curPos.X > 0) {
						if (!selected) { curPos.X--; }
						else if ( Swap(curPos, SWAP_DIRECTION_LEFT) > 0 ) {
							curPos.X--; isProcessMatch = true;
						}
					}
					break;
				case KEY_DOWN:
					if (curPos.Y < BOARD_HEIGHT -1) {
						if (!selected) { curPos.Y++; }
						else if ( Swap(curPos, SWAP_DIRECTION_DOWN) > 0 ) {
							curPos.Y++; isProcessMatch = true;
						}
					}
					break;
				case KEY_UP:
					if (curPos.Y > 0) {
						if (!selected) { curPos.Y--; }
						else if ( Swap(curPos, SWAP_DIRECTION_UP) > 0 ) {
							curPos.Y--; isProcessMatch = true;
						}
					}
					break;
				case KEY_ENTER:
				case KEY_SPACE:
					if ( !selected && _gems[curPos.X][curPos.Y].s == GEM_TYPE_CANDY ) {
						ProcessCandyGem ( _gems[curPos.X][curPos.Y] );
					} else {
						selected = !selected;
					}
					break;
				case KEY_ESCAPE:
					return false;
					break;

			}
		} else if ( ie.mouseClicked ) {
			curPos.X = ( ie.pos.X - LEFT_MARGIN ) / 6;
			curPos.Y = ( ie.pos.Y - TOP_MARGIN ) / 3;
			
			if ( curPos.X >= 0 && curPos.X < BOARD_WIDTH &&
				 curPos.Y >= 0 && curPos.Y < BOARD_HEIGHT ) {
				if ( selected ) {
					if ( Swap( oldPos, curPos ) > 0 ) {
						isProcessMatch = true;
					} else {
						curPos = oldPos;
					}
					selected = false;
				} else {
					if ( _gems[curPos.X][curPos.Y].s == GEM_TYPE_CANDY ) {
						selected = false;
						ProcessCandyGem ( _gems[curPos.X][curPos.Y] );
					} else {
						selected = true;
					}
				}
			} else {
				curPos = oldPos;
			}
		}
		// カーソルを描く
		_gems[curPos.X][curPos.Y].redraw = true;
		_gems[oldPos.X][oldPos.Y].redraw = true;

		// 変更したブロックをまとめて処理
		if ( isProcessMatch ) {
			// 影響を与えた行
			chgCols = 1 << curPos.X;
			chgCols |= 1 << oldPos.X;
			chgRows = 1 << curPos.Y;
			chgRows |= 1 << oldPos.Y;

			// ブロックを消す
			PostProcessSwap( oldPos, curPos );
			
			// UIおよび画面の処理
			selected = false;
			_gems[curPos.X][curPos.Y].redraw = true;
			_gems[oldPos.X][oldPos.Y].redraw = true;

			if ( comboGaugeValue >= COMBO_GAUGE_LEN ) {
				GenerateCandyGem();
				comboGaugeValue = 0;
			}
		}
		return true ;

	}
	return true;
}

/*
 * キャンディーブロックを生成
 */
void GameMain::GenerateCandyGem()
{	
	int _x, _y;
	do {
		_x = rand() % (int)BOARD_WIDTH;
		_y = rand() % (int)BOARD_HEIGHT;
	} while ( _gems[_x][_y].s != GEM_TYPE_NORMAL );

	_gems[_x][_y].s   = GEM_TYPE_CANDY;
	_gems[_x][_y].id  = SPECIAL_GEM_ID;
	_gems[_x][_y].bid = GetRandGemID();
	DrawComboGauge();
}

/*
 * Ready Go アニメを表示する
 */

void GameMain::DrawReadyGo()
{
	const char illumText[2][7][50] = {
	{
		" ______    _______  _______  ______   __   __ ",
		"|    _ |  |       ||   _   ||      | |  | |  |",
		"|   | ||  |    ___||  |_|  ||  _    ||  |_|  |",
		"|   |_||_ |   |___ |       || | |   ||       |",
		"|    __  ||    ___||       || |_|   ||_     _|",
		"|   |  | ||   |___ |   _   ||       |  |   |  ",
		"|___|  |_||_______||__| |__||______|   |___|  "
	},
	{
		"                _______  _______                \n",
		"               |       ||       |               \n",
		"               |    ___||   _   |               \n",
		"               |   | __ |  | |  |               \n",
		"               |   ||  ||  |_|  |               \n",
		"               |   |_| ||       |               \n",
		"               |_______||_______|               \n"
	} };

	int l = 17;
	int t = 11;
	int tl = 50;
	om->SetColor ( COLOR_YELLOW );
	int k = gm->GetFrameCount();
	int ti = k >= READY_DURATION ? 1 : 0;
	int kHalf =  k >= READY_DURATION ? READY_DURATION / 2 : GO_DURATION / 2;
	int a;

	if ( k > READY_DURATION ) k -= READY_DURATION;

	// 無駄なRedrawを回避する
	if (  ( k < kHalf && k*4 - 1 > l + tl ) ||
		  ( k >= kHalf && ( k - kHalf )*4 + 7 + l + tl < l + tl )
		) return;

	// 演出の描画
	for ( int i = 0; i < 7; i++ ) {
		a = k < kHalf ? min( k*4 + i, l + tl ) : max ( ( k - kHalf )*4 + i + l + tl, l + tl );
		om->GotoXY ( 0, t + i );
		om->Print ( "%*s" "%s", max( a - tl, 0) , " " );
		om->Print ( "%.*s", max( tl - max ( a - SCREEN_WIDTH, 0 ), 0 ) , illumText[ti][i] + max( 0 , tl-a ) );
		om->Print ( "%*s" "%s", max( 0, SCREEN_WIDTH + tl - k  ) , " " );
	}

	om->SetColor ( COLOR_LIGHT_GREEN );
	om->Print ( l, t-2, "================================================" );
	om->Print ( l, t-1, "                                                " );
	om->Print ( l, t+7, "                                                " );
	om->Print ( l, t+8, "                                                " );
	om->Print ( l, t+9, "================================================" );
}


/*
 * ブロックの描画
 */
void GameMain::Redraw( bool isForceRedraw )
{
	bool isDirty = false;
	uchar delBC = BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
	if ( lastDelete > 0 ) {
		if ( lastDelete >= DELETE_KEY_FRAME_1 ) delBC = BACKGROUND_GREEN | BACKGROUND_BLUE ;
		else if ( lastDelete >= DELETE_KEY_FRAME_2 ) delBC = BACKGROUND_INTENSITY ;
	}

	int fc = gm->GetFrameCount();

	for (int i=0; i < BOARD_WIDTH; i++) {
		for (int j=0; j < BOARD_HEIGHT; j++) {
			// 差分のみを描画する
			if ( !(_gems[i][j].redraw || _gems[i][j].del ) &&
				 _gems[i][j].s != GEM_TYPE_CANDY &&
				 !( hintPos.X == i && hintPos.Y == j ) &&
				 !isForceRedraw
				 ) continue;
			isDirty = true;

			// 色を設定する
			uchar id = _gems[i][j].id;
			if ( _gems[i][j].s == GEM_TYPE_CANDY ) {
				if ( fc % 8 == 0 ) {
					// 個数が>ceiling(BOARD_WIDTH/2.0f) のブロックのみを消せる →　
					//     楽しさ　と　落下時に絶対消せることを数学上に保証するため
					//                          ↓
					//                 二個連続のパタンが必須
					//                          ↓
					//  たとえば、BOARD_WIDTH = 7　および　個数が<=4を消す場合
					//  Falldownが終わって、最上段が　●○●○●○●  になる可能性がある　（●は消されたブロック）
					//  二連続のパタンが存在しないため、絶対消せる保証もできなくなる
					do { id = GetRandGemID(); } while ( gemStat[id] <= ( BOARD_WIDTH / 2 + BOARD_WIDTH % 2 ) );
					_gems[i][j].bid = id;
				} else if ( animCols == 0 ) {
					continue;
				} else {
					id = _gems[i][j].bid;
				}
			}
			uchar c = colors[id];

			int ni = ( i * 6 ) + LEFT_MARGIN ;
			int nj = ( j * 3 ) - _gems[i][j].yOffset + TOP_MARGIN;
			
			if ( animCols != 0 ) {
				// 落下操作
				int _floor = _gems[i][j].yOffset - ( j == BOARD_HEIGHT - 1 ? 0 :  _gems[i][j+1].yOffset );
				for (int k = 1; k <= _floor + 2; k++ ) {
					if ( k + nj < TOP_MARGIN ) continue;
					om->Print( ni, k + nj, "      " );
				}
			} else {
				if ( _gems[i][j].del && lastDelete > 0 ) {
					c = lastDelete < DELETE_KEY_FRAME_2 ? COLOR_GRAY | BACKGROUND_INTENSITY  : c|delBC ;
				} else if ( i == curPos.X && j == curPos.Y ) {
					if ( selected ) c |= BACKGROUND_RED;
					else if ( im->IsUsingKeyboard() ) c |= BACKGROUND_INTENSITY;
				} else if ( i == hintPos.X && j == hintPos.Y ) {
					c |= ( ( fc / 40 ) % 2 == 0 ? BACKGROUND_GREEN : COLOR_CLEAR ) ;
				}
			}
			om->SetColor ( c );

			// ブロックの種類により描画する
			switch (_gems[i][j].s) {
				case GEM_TYPE_NORMAL:
					if (nj >= TOP_MARGIN ) {
						om->Print ( ni, nj, "┏━┓");
					}
					if (nj+1 >= TOP_MARGIN ) {
						om->Print ( ni,   nj+1, "┃");
						om->Print ( ni+2, nj+1, tx[id]);
						om->Print ( ni+4, nj+1, "┃");
					}
					if (nj+2 >= TOP_MARGIN ) {
						om->Print ( ni, nj+2, "┗━┛" );
					}
					break;
				case GEM_TYPE_FOUR:
					om->Print ( ni,   nj,   "□□□" );
					om->Print ( ni,   nj+1, "□" );
					om->Print ( ni+2, nj+1, tx[id]);
					om->Print ( ni+4, nj+1, "□" );
					om->Print ( ni,   nj+2, "□□□");
					break;
				case GEM_TYPE_FIVE:
					om->Print ( ni,   nj,   "☆☆☆" );
					om->Print ( ni,   nj+1, "☆" );
					om->Print ( ni+2, nj+1, tx[id]);
					om->Print ( ni+4, nj+1, "☆" );
					om->Print ( ni,   nj+2, "☆☆☆");
					break;
				case GEM_TYPE_CANDY:
					// TODO 汚い?
					om->GotoXY(ni,nj);   om->Print (tx[id]);om->Print (tx[id]);om->Print (tx[id]);
					om->GotoXY(ni,nj+1); om->Print (tx[id]);om->Print (tx[id]);om->Print (tx[id]);
					om->GotoXY(ni,nj+2); om->Print (tx[id]);om->Print (tx[id]);om->Print (tx[id]);
					break;
			}
			if ( _gems[i][j].yOffset == 0 ) _gems[i][j].redraw = false;
		}
	}

#ifdef _DEBUG
	om->SetColor(COLOR_YELLOW);
	om->Print ( LEFT_MARGIN, BOARD_HEIGHT*3+9, "Frame = %8d(%2d) FPS=%3.2f", fc, gm->GetFrameDuration(), gm->GetFPS () );
	om->GotoXY(LEFT_MARGIN, BOARD_HEIGHT*3+10);
	om->Print("x=%d, y=%d, id=%d, s=%d, hc=%d, vc=%d, del=%d, yo=%d   ", curPos.X, curPos.Y, _gems[curPos.X][curPos.Y].id, _gems[curPos.X][curPos.Y].s, _gems[curPos.X][curPos.Y].hc, _gems[curPos.X][curPos.Y].vc, _gems[curPos.X][curPos.Y].del, _gems[curPos.X][curPos.Y].yOffset);
	om->GotoXY(LEFT_MARGIN, BOARD_HEIGHT*3+11);
	om->Print ( "ld= %4d, ac= %4d, cc= %4d, cr= %4d     ", lastDelete, animCols, chgCols, chgRows );
	om->GotoXY(LEFT_MARGIN, BOARD_HEIGHT*3+12);
	om->Print ("Count:");
	for ( int i = 0; i < BOARD_GEM_COUNT; i++ ) {
		om->Print (tx[i]);
		om->Print ("=%2d ", gemStat[i]);
	}
	
	om->GotoXY(LEFT_MARGIN, BOARD_HEIGHT*3+13);
	om->Print (" Prob:");
	for ( int i = 0; i < BOARD_GEM_COUNT; i++ ) {
		om->Print (tx[i]);
		om->Print ("=%2d ", gemProbTable[i]);
	}
#endif

}

/*
 * 消える・落ちるのアニメの各フレームを処理
 */
void GameMain::ProcessAnim ()
{
	int fd = gm->GetFrameDuration();
	if ( lastDelete > 0 ) {
		lastDelete -= fd;
		if ( lastDelete <= 0 ) {
			ProcessFalldown ();
			lastDelete = 0;
		}
	} else if ( animCols !=0 ) {
		int i, j;
		for ( i = 0; i < BOARD_WIDTH; i++ ) {
			if ( ( chgCols & ( 1 << i ) ) == 0 ) continue;
			// 各列に対し、下から走査
			for ( j = 0; j < BOARD_HEIGHT; j++ ){
				if ( _gems[i][j].yOffset > 0 ) {
					_gems[i][j].yOffset = max ( _gems[i][j].yOffset - fd , 0 ) ;
				} else if ( j == 0 ) {
					animCols &= ~(1 << i);
					break;
				} else {
					break;
				}
			}
		}
		if ( animCols == 0 ) tm->Pause ( false );
	}
}


/*
 * 消せるブロックがないまでに落下処理する（アニメなし）
 */
void GameMain::ProcessAllMatch( )
{
	while ( chgCols != 0 ) {
		ProcessMatchLines ();
		ProcessFalldown ( false );
	}
}

COORD GameMain::FindMostSwapableGem ()
{
	int i=0, j=0, k;
	int mc = 0; // 最も交換できる数1
#ifdef _DEBUG
	int c = 0;
#endif


	// 最も交換できるブロックの座標の連続個数
	COORD bestPos = { -1, -1 }, oldPos, newPos;
	int bestCount = 0;

	// 最も交換できる選択肢から一様分布に従い、ランダムに一個を選ぶ
	// ALOGRITHM ：　 最大連続数 = 0；　交換できるスワップの個数 = 0；　現選択肢 = なし
	//                for 各行と列
	//                  if 交換できるスワップであれば:
	//                      交換できるスワップの個数++
	//                      1/ 交換できるスワップの個数の確率で、現選択枝 = このスワップ
	// このアルゴリズムはスワップの選択肢が一様分布に従うことを保証できる
	// 数学上の証明は略
	// 例えば
	// 最初 A が選ばれ、 Bが1/2の確率で結果を入れ替える、Cが1/3の確率で結果を入れ替えるとしたら、こういう感じになる
	// A   B   C
	// 1   0   1 -> C
	// 1   0   0 -> A
	// 1   0   0 -> A
	// 0   1   1 -> C
	// 0   1   0 -> B
	// 0   1   0 -> B

	// // プログラムで証明
	// int _k = 0, i, j;
	// int l = 10;
	// int _s[10];
	// for ( i = 0; i < l; i++) { _s[i] = 0; }
	// for ( j = 0; j< 100000; j++) {
	//	for ( i = 0; i < l; i++) {
	//		if ( rand() % ( i + 1 ) == 0 ) _k = i;
	//	}
	//	_s[_k] ++;
	// }
	
	for ( i = 0; i < BOARD_WIDTH; i++) {
		for ( j = 0; j < BOARD_HEIGHT - 1; j++){
			oldPos.X = i; oldPos.Y = j;
			newPos.X = i; newPos.Y = j+1;
			k = Swap ( oldPos, newPos, true );
#ifdef _DEBUG
			if ( k > 0 ) c++;
#endif
			if ( k > bestCount ) {
				mc = 1;
				bestCount = k;
				bestPos = oldPos;
			} else if ( k == bestCount ) {
				mc ++;
				if ( rand() % mc == 0 ) bestPos = oldPos;
			}
		}
	}
	for ( j = 0; j < BOARD_HEIGHT; j++){
		for ( i = 0; i < BOARD_WIDTH - 1; i++ ) {
			oldPos.X = i;   oldPos.Y = j;
			newPos.X = i+1; newPos.Y = j;
			k = Swap ( oldPos, newPos, true );
#ifdef _DEBUG
			if ( k > 0 ) c++;
#endif
			if ( k > bestCount ) {
				mc = 1;
				bestCount = k;
				bestPos = oldPos;
			} else if ( k == bestCount ) {
				mc ++;
				if ( rand() % mc == 0 ) bestPos = oldPos;
			}
		}
	}
#ifdef _DEBUG
	om->SetColor ( COLOR_WHITE );
	om->Print ( LEFT_MARGIN, 1, "actPat=%3d", c );
#endif

	return bestPos;
}

/*
 * 各行列に並んでいるブロックにマークを付けて消して落とし、
 * 空きスペースにランダムにブロックを補充する
 * isCheckLinesがfalseであれば、行列チェックはしない
 *
 * @return 消せるブロックがなければfalse、あればtrue
 */
bool GameMain::ProcessMatchLines()
{
	//  走査用変数
	int k = 0, a = 0;
	int i = 0, j = 0;
	int _chgCols = 0, _chgRows = 0;

	// 各行: 連続個数を計算
	for ( j = 0; j < BOARD_HEIGHT; j++){
		// 触れていない行をスキップ
		if ( ( chgRows & ( 1 << j ) ) == 0 ) continue;

		i = 0;
		do {
			k = 1;
			while (_gems[i+k][j].id == _gems[i][j].id) {
				k++;
				if ( i+k >= BOARD_WIDTH ) break;
			}
			if ( k >= 3 ) {
				for (a = 0; a < k; a++) {
					_gems[i+a][j].hc = k;
					_gems[i+a][j].del = true;
					_gems[i+a][j].redraw = true;
					_chgCols |= 1 << ( i + a );
					_chgRows |= 1 << j;
				}
			}
			i += k;
		}while( i < BOARD_WIDTH - 1 );
	}

	// 各列: 連続個数を計算
	for ( i = 0; i < BOARD_WIDTH; i++){
		// 触れていない列をスキップ
		if ( ( chgCols & ( 1 << i ) ) == 0 ) continue;

		j = 0;
		do {
			k = 1;
			while (_gems[i][j+k].id == _gems[i][j].id) {
				k++;
				if ( j+k >= BOARD_HEIGHT ) break;
			}

			if ( k >= 3 ) {
				for (a = 0; a < k; a++) {
					_gems[i][j+a].vc = k;
					_gems[i][j+a].del = true;
					_gems[i][j+a].redraw = true;
					_chgCols |= 1 << i;
					_chgRows |= 1 << ( j + a );
				}
			}
			j += k;
		}while( j < BOARD_HEIGHT - 1 );
	}

	// 消せるブロックがなければそのままで戻る
	chgCols = _chgCols;
	chgRows = _chgRows;
	if ( chgCols == 0 && chgRows == 0 ) return false;

	// 特殊ブロックが消される時の処理、特殊ブロックの生成、各列走査
	for ( i = 0; i < BOARD_WIDTH; i++){
		for ( j = 0; j < BOARD_HEIGHT; j++){
			if ( _gems[i][j].del && _gems[i][j].s != GEM_TYPE_NORMAL ) {
				ProcessSpecialGemEffect( i, j );
			}

			// 特殊ブロックを生成する
			if ( _gems[i][j].hc == 3 && _gems[i][j].vc == 3 ) { // 交差点の五連続、チェックは一回のみ
				_gems[i][j].s = GEM_TYPE_FIVE;
				_gems[i][j].del = false;
			} else {
				// 各列の特殊ブロックを生成
				if (_gems[i][j].vc > 3) {
					SHORT _y;
					_y = j + rand() % _gems[i][j].vc;
					// 触ったところをチェック
					for ( uchar _m = 0; _m < 2; _m++ ) {
						if (pos[_m].X == i && pos[_m].Y >= j && pos[_m].Y < j + _gems[i][j].vc) {
							_y = pos[_m].Y;
							break;
						}
					}
					// 特殊ブロック生成
					_gems[i][_y].s = _gems[i][j].vc == 4 ? GEM_TYPE_FOUR : GEM_TYPE_FIVE;
					_gems[i][_y].del = false;
					j += _gems[i][j].vc - 1;
				}
			}
		}
	}
	// 各行走査
	for ( j = 0; j < BOARD_HEIGHT; j++){
		for ( i = 0; i < BOARD_WIDTH; i++){
			// 各行の特殊ブロックを生成
			if (_gems[i][j].hc > 3) {
				SHORT _x;
				_x = i + rand() % _gems[i][j].hc;
				// 触ったところをチェック
				for ( uchar _m = 0; _m < 2; _m++ ) {
					if (pos[_m].Y == j && pos[_m].X >= i && pos[_m].X < i + _gems[i][j].hc) {
						_x = pos[_m].X;
						break;
					}
				}
				// 特殊ブロック生成
				_gems[_x][j].s = _gems[i][j].hc == 4 ? GEM_TYPE_FOUR : GEM_TYPE_FIVE;
				_gems[_x][j].del = false;
				i += _gems[i][j].hc - 1;
			}
			// キャンディーブロックのための走査
			else if ( _gems[i][j].s == GEM_TYPE_CANDY ) {
				_gems[i][j].del = false;
			}
		}
	}
	tm->Pause ( true );
	lastDelete = DELETE_ANIM_DURATION;
	return true;
}

/*
 * delがtrueであるブロックを採点し落として、新しいブロックを生成
 */
void GameMain::ProcessFalldown ( bool withAnim )
{	
	//  走査用変数
	int k = 0;
	int i = 0, j = 0;
	int changedCount[BOARD_WIDTH];

	// 連続のブロックを消し、下に詰める

	chgCols = 0;
	chgRows = 0;
	int s = 0;
	int dc = 0;
	for ( i = 0; i < BOARD_WIDTH; i++ ) {
		changedCount[i] = 0;
		k = BOARD_HEIGHT - 1; // 積む位置
		for ( j = k; j >= 0; j-- ) {
			// スコアを計算
			if ( _gems[i][j].del ) {
				//
				// 3連続 → 300点
				// 4連続 → 600点
				// 5連続 → 1500点
				//
				// TODO 仕様要確認
				//      ○　←　黒と交換
				//  ○○●○○
				//  　　○
				//    　○
				// のような３方向のを正しく処理できない。
				// LINE POPではこのようなパタンを分解し、
				// 長い列を優先に処理し○○●○○のみを計算するらしい（なかなか再現できない）
				if ( _gems[i][j].hc == 3 && _gems[i][j].vc == 3 ) { // 二つの3連続の接続部分
					s += 12;
				} else if ( _gems[i][j].hc >= 3 || _gems[i][j].vc >= 3 ) { // 長い列なら
					// TODO 念のため、縦と横をそれぞれ集計
					switch( _gems[i][j].hc ) {
						case 5: s+= 4; break; // 5*4 = 20 点
						case 4: s+= 3; break; // 4*3 = 12 点
						case 3: s+= 2; break; // 3*2 = 6 点
					}
					switch( _gems[i][j].vc ) {
						case 5: s+= 4; break; // 5*4 = 20 点
						case 4: s+= 3; break; // 4*3 = 12 点
						case 3: s+= 2; break; // 3*2 = 6 点
					}
				} else { // 特殊ブロックの消去による消えたブラック
					s += 1;
					dc ++;
				}
				_gems[i][j].s = GEM_TYPE_NORMAL;
				_gems[i][j].redraw = true;
				chgRows |= 1 << j;

			}
			// 保留するブロックなら
			else {
				if ( k != j ) {
					_gems[i][k].id = _gems[i][j].id; //下に詰める
					_gems[i][k].yOffset =  withAnim ? (k - j) * 3 : 0; 
					_gems[i][k].hc = 1;
					_gems[i][k].vc = 1;
					_gems[i][k].s = _gems[i][j].s;
					_gems[i][k].redraw = true;
					_gems[i][j].redraw = true;
					chgRows |= 1 << j;
					chgRows |= 1 << k;
				}
				k--;
			}
			// 消すべきのを消す
			_gems[i][j].hc = 1;
			_gems[i][j].vc = 1;
			_gems[i][j].del = false;
		}
	

		// ランダムにブロックを入れている
		int l = k + 1; // 生成するブロック数
		changedCount[i] = l;

		while ( k >= 0 ) {
			_gems[i][k].yOffset = withAnim ? l * 3 : 0; 
			_gems[i][k].id = GetRandGemID( true );
			_gems[i][k].s = GEM_TYPE_NORMAL;
			_gems[i][k].redraw = true;

			chgCols |= 1 << i;
			k--;
		}
	}
	s *= 50;
	// 特殊の影響範囲が大きければ大きいほど、点数が高い
	s += dc * dc * 10;
	// コンボ数が大きければ大きいほど、点数が高い
	s += comboCount * comboCount * 10; 
	// フィーバータイム中消すと、なんと点数倍増！
	s *= isFeverMode ? 2 : 1;
	IncScore( s );

	if ( chgCols != 0 ) {
		// TODO キャンディーブロックが消された場合、二個連続パタンが存在しない可能性もある
		// 絶対消せることを保証
		//  STEP 1 : 新しい追加分のすべての二個連続パタンからひとつを選択
		//
		//    ○○　　または　　○
		//　　　　　　　　　　　○
		//
		
		COORD s; // パタンの左上の座標
		s.X = -1; s.Y = -1;
		int patternCount = 0;
		int dir = -1; // 0=縦 1=横 
		
		// 縦走査
		int depth = 0;
		for ( i = 0; i < BOARD_WIDTH; i++ ) {
			depth = max ( changedCount[i], depth );
			if ( changedCount[i] < 2 ) continue;
			for ( j = 0; j < changedCount[i] - 1; j++ ) {
				patternCount ++ ;
				if ( rand() % patternCount == 0 ) {
					s.X = i; s.Y = j; dir = 0;
				}
			}
		}

		// 横走査
		for ( j = 0; j < depth; j++ ) {
			for ( i = 0; i < BOARD_WIDTH - 1 ; i++ ) {
				if ( changedCount[i] > j && changedCount[i+1] > j ) {
					patternCount ++;
					if ( rand() % patternCount == 0 ) {
						s.X = i; s.Y = j; dir = 1;
					}
				}
			}
		}

		// STEP 2 : 選ばれたパタンの回りにひとつの点を選んで消せるパタンを生成

		// 黒●の点をひとつ選び、白○の色をその点の色にする
		// 縦の２連続に対し
		//     ●
		//   ●　●
		//     ○
		//     ○
		//   ●　●
		//     ●

		// 横の２連続に対し
		//     ●　　●
		//   ●　○○　●
		//     ●　　●

		const COORD Pat[2][6] = {
			{ { -1, -1 }, { 0, -2 }, { 1, -1 }, { -1, 2 }, { 0, 3 }, { 1, 2 } },
			{ { -1, -1 }, { -2, 0 }, { 1, -1 }, { 2, -1 }, { 3, 0 }, { 2, 1 } }
		};
		patternCount = 0;
		COORD b;
		for ( i = 0; i < 6; i++ ) {
			if  ( Pat[dir][i].X + s.X >= 0 && Pat[dir][i].X + s.X < BOARD_WIDTH &&
				  Pat[dir][i].Y + s.Y >= 0 && Pat[dir][i].Y + s.Y < BOARD_HEIGHT ) {
				if ( _gems[Pat[dir][i].X + s.X][Pat[dir][i].Y + s.Y].s == GEM_TYPE_CANDY ) continue;
				patternCount ++;
				if ( rand() % patternCount == 0 ) {
					b.X = Pat[dir][i].X + s.X;
					b.Y = Pat[dir][i].Y + s.Y;
				}
			}
		}
		// TODO ２個パタンのない場合も対応
		if ( patternCount > 0 ) {
			uchar s_id = _gems[b.X][b.Y].id;
			_gems[s.X][s.Y].id = s_id;
			if ( dir == 0 ) { _gems[s.X][s.Y+1].id = s_id; }
			else { _gems[s.X+1][s.Y].id = s_id; }
		}

#ifdef _DEBUG
		om->Print ( (uchar)COLOR_WHITE, LEFT_MARGIN, 0, "x=%4d,y=%4d,dir=%4d, pc=%d, bx=%d, by=%d", s.X, s.Y, dir, patternCount, b.X, b.Y );
#endif
	}

	// 落下演出
	animCols = withAnim ? chgCols : 0;
	if ( hintPos.X >= 0 && hintPos.Y >= 0 ) {
		_gems[hintPos.X][hintPos.Y].redraw = true;
		hintPos.X = -1;
		hintPos.Y = -1;
	}

	TotalUpGems();

	return;
}



/*
 * ある点をある方向と交換する
 * isForceRecoverはtrueならば、強制復旧する
 * @return 交換できなければfalse
 */

int GameMain::Swap( COORD oldPos, SWAP_DIRECTION d, bool isForceRecover) {
	COORD newPos = oldPos;
	// 移動方向
	switch ( d ) {
		case SWAP_DIRECTION_LEFT:  newPos.X--; break;
		case SWAP_DIRECTION_RIGHT: newPos.X++; break;
		case SWAP_DIRECTION_UP:    newPos.Y--; break;
		case SWAP_DIRECTION_DOWN:  newPos.Y++; break;
	}
	return Swap( oldPos, newPos, isForceRecover );
}

int GameMain::Swap ( COORD oldPos, COORD newPos, bool isForceRecover ) {
	// スワップに関わった点
	pos[0] = oldPos;
	pos[1] = newPos;
	GEM gs[2];

	// 交換できるかどうかをチェック
	if ( abs ( oldPos.X - newPos.X ) + abs ( oldPos.Y - newPos.Y ) != 1 )
		return false;

	// フラグ初期化
	chgCols = 0;

	gs[0] = _gems[pos[0].X][pos[0].Y];
	gs[1] = _gems[pos[1].X][pos[1].Y];

	// 同じブロックなら交換できない
	if ( gs[0].id == gs[1].id ) return false;
	// キャンディーブロックと交換できない
	if ( gs[0].s == GEM_TYPE_CANDY || gs[1].s == GEM_TYPE_CANDY ) return false;

	// 交換してみる
	_gems[pos[0].X][pos[0].Y] = gs[1];
	_gems[pos[1].X][pos[1].Y] = gs[0];

	//　関わった点の回りに３連続があるかどうかを確認
	uchar nc;

	int res = 0, i, j;
	int v, h ;
	// 二つの点
	for(int m=0; m<2; m++) {
		nc = _gems[pos[m].X][pos[m].Y].id;
		v = 1; h = 1;

		// 左右上下の連続数
		for ( i = pos[m].X+1; i < BOARD_WIDTH  && _gems[i][pos[m].Y].id == nc ; i++ )  { h++; }
		for ( i = pos[m].X-1; i >= 0           && _gems[i][pos[m].Y].id == nc ; i-- )  { h++; }
		for ( j = pos[m].Y+1; j < BOARD_HEIGHT && _gems[pos[m].X][j].id == nc ; j++ )  { v++; }
		for ( j = pos[m].Y-1; j >= 0           && _gems[pos[m].X][j].id == nc ; j-- )  { v++; }

		if ( h >= 3 && v >= 3 ) res = max( 5, res );
		else if ( h >= 3 || v >= 3 ) res = max( max( v, h ), res );
	}

	// 復旧する
	if ( isForceRecover || res == 0 ) { 
		_gems[pos[0].X][pos[0].Y] = gs[0];
		_gems[pos[1].X][pos[1].Y] = gs[1];
	}
	return res;
}

void GameMain::PostProcessSwap ( COORD oldPos, COORD newPos )
{
	// 交換履歴を保存する
	unsigned long cur = tm->CurrentTick();

	lastSwap = cur;
	if ( hintPos.X >= 0 && hintPos.Y >= 0 ) {
		_gems[hintPos.X][hintPos.Y].redraw = true;
		hintPos.X = -1;
		hintPos.Y = -1;
	}
	
	// コンボしたかをチェックする
	if ( comboLog.size() > 0 ) {
		if ( cur - comboLog.back() <= COMBO_THRESHOLD_TIME ) {
			comboCount ++;
			comboGaugeValue ++;

			lastComboGaugeChanged = cur;
			DrawComboGauge();
		}
	}
	comboLog.push( cur );
	
	// 不要なコンボ履歴を消す
	while ( comboLog.size() > FEVER_THRESHOLD_COMBO_COUNT ) {
		comboLog.pop();
	}

	// フィーバータイム中なら移動したブロックを特殊ブロックとする
	if (isFeverMode) {
		ProcessSpecialGemEffect ( pos[1].X, pos[1].Y, GEM_TYPE_FOUR );
	}

	// 条件を満たせばフィーバータイムに入る
	if ( comboCount >= FEVER_THRESHOLD_COMBO_COUNT && comboLog.front() < cur + FEVER_THRESHOLD_TIME  ) {
		ChangeFeverTimeState(true);
		comboLog = std::queue<unsigned long>();
		comboLog.push( cur );
	}
}

/*
 * ランダムにブロックのIDを生成
 */
uchar GameMain::GetRandGemID( bool isAutoBalance )
{
	if ( isAutoBalance ) {
		// 確率テーブルによりランダム数を生成
		int r = rand () % gemProbTable[ BOARD_GEM_COUNT - 1 ];
		for ( uchar k = 0; k < BOARD_GEM_COUNT; k++ ) {
			if ( r < gemProbTable[ k ]  ) return k;
		}
	} else {
		return rand() % (int)(BOARD_GEM_COUNT);
	}
	return 0;
}

/*
 * ある位置にいるブロックが消える時に行う処理
 *
 * @args x,y ブロックの座標
 *       t   処理方法を指定する、指定しない場合、そのブロックの種類によって処理する
 */
void GameMain::ProcessSpecialGemEffect(int x, int y, GEM_TYPE t) {
	int k;
	GEM_TYPE s = t == GEM_TYPE_UNKNOWN ? _gems[x][y].s : t;
	switch ( s ) {
		case GEM_TYPE_FIVE:
			{
				// 処理した後にノーマルに戻す
				if ( t == GEM_TYPE_UNKNOWN ) _gems[x][y].s = GEM_TYPE_NORMAL;
				for ( k = 0; k < BOARD_WIDTH; k++ )  { 
					_gems[k][y].del = true;
					_gems[k][y].redraw = true;
					if ( _gems[k][y].s != GEM_TYPE_NORMAL ) ProcessSpecialGemEffect( k, y );
				}
				for ( k = 0; k < BOARD_HEIGHT; k++ ) {
					_gems[x][k].del = true;
					_gems[x][k].redraw = true;
					if ( _gems[x][k].s != GEM_TYPE_NORMAL ) ProcessSpecialGemEffect( x, k );
				}

				break;
			}
		case GEM_TYPE_FOUR:
			{
				// 処理した後にノーマルに戻す
				if ( t == GEM_TYPE_UNKNOWN ) _gems[x][y].s = GEM_TYPE_NORMAL;
				COORD leftTopPos = { max( 0, x - 1 ), max( 0, y - 1) };
				COORD rightBottomPos = { min( BOARD_WIDTH - 1, x + 1 ), min( BOARD_HEIGHT - 1, y + 1 ) };
				for (int _x = leftTopPos.X; _x <= rightBottomPos.X; _x++ ) {
					for (int _y = leftTopPos.Y; _y <= rightBottomPos.Y; _y++ ) {
						_gems[_x][_y].del = true;
						_gems[_x][_y].redraw = true;
						if ( _gems[_x][_y].s != GEM_TYPE_NORMAL ) ProcessSpecialGemEffect( _x, _y );
					}
				}
				break;
			}
		case GEM_TYPE_CANDY:
			_gems[x][y].del = false;
			break;
	}
}

void GameMain::ProcessCandyGem ( GEM &g )
{
	g.del = true;
	g.redraw = true;
	uchar id = g.bid;

	for ( int _x = 0; _x < BOARD_WIDTH; _x++ ) {
		for ( int _y = 0; _y < BOARD_WIDTH; _y++ ) {
			if ( _gems[_x][_y].id == id && _gems[_x][_y].s == GEM_TYPE_NORMAL ) {
				_gems[_x][_y].del = true;
				_gems[_x][_y].redraw = true;
			}
		}
	}
	tm->Pause(true);
	lastDelete = DELETE_ANIM_DURATION;
}