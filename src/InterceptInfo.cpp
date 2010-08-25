/************************************************************************************
 * WrightEagle (Soccer Simulation League 2D)                                        *
 * BASE SOURCE CODE RELEASE 2010                                                    *
 * Copyright (C) 1998-2010 WrightEagle 2D Soccer Simulation Team,                   *
 *                         Multi-Agent Systems Lab.,                                *
 *                         School of Computer Science and Technology,               *
 *                         University of Science and Technology of China, China.    *
 *                                                                                  *
 * This program is free software; you can redistribute it and/or                    *
 * modify it under the terms of the GNU General Public License                      *
 * as published by the Free Software Foundation; either version 2                   *
 * of the License, or (at your option) any later version.                           *
 *                                                                                  *
 * This program is distributed in the hope that it will be useful,                  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 * GNU General Public License for more details.                                     *
 *                                                                                  *
 * You should have received a copy of the GNU General Public License                *
 * along with this program; if not, write to the Free Software                      *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.  *
 ************************************************************************************/

#include <cstdlib>
#include "InterceptInfo.h"
#include "InterceptModel.h"
#include "Dasher.h"
#include "Logger.h"
#include <algorithm>

InterceptInfo::InterceptInfo(WorldState *pWorldState, InfoState *pInfoState): InfoStateBase( pWorldState, pInfoState )
{
}

void InterceptInfo::UpdateRoutine()
{
	SortIntercerptInfo();
}

void InterceptInfo::SortIntercerptInfo()
{
	mOIT.clear();

	for (int i = -TEAMSIZE; i <= TEAMSIZE; i++){
		if (i == 0) continue;

		if (mpWorldState->GetPlayer(i).IsAlive()){
			mOIT.push_back(OrderedIT(VerifyIntInfo(i), i, mpWorldState->GetPlayer(i).GetPosDelay()));
		}
	}

	std::sort(mOIT.begin(), mOIT.end());

	if (PlayerParam::instance().SaveTextLog()) {
		Logger::instance().GetTextLogger("oit") << "\n" << mpWorldState->CurrentTime() << ": "<< std::endl;
		Logger::instance().GetTextLogger("oit") << "#" << "\t" << "cd" << "\t" << "min" << "\t" << "idl" << "\t" << "its" << "\t" << "it0" << "\t" << "it1" << "\t" << "it2" << std::endl;

		for (std::vector<OrderedIT>::iterator it = mOIT.begin(); it != mOIT.end(); ++it) {
			Logger::instance().GetTextLogger("oit") << it->mUnum
			<< "\t" << it->mCycleDelay
			<< "\t" << it->mpInterceptInfo->mMinCycle
			<< "\t" << it->mpInterceptInfo->mIntervals;

			for (int i = 0; i < it->mpInterceptInfo->mIntervals; ++i) {
				Logger::instance().GetTextLogger("oit") << "\t" << it->mpInterceptInfo->mInterCycle[i];
			}

			Logger::instance().GetTextLogger("oit") << std::endl;
		}
	}
}

PlayerInterceptInfo *InterceptInfo::VerifyIntInfo(Unum unum)
{
	PlayerInterceptInfo* pInfo = GetPlayerInterceptInfo(unum);
	if (!pInfo) {
		return 0;
	}

	if(pInfo->mTime != mpWorldState->CurrentTime()){
		CalcTightInterception(mpWorldState->GetBall(), pInfo);
		pInfo->mTime = mpWorldState->CurrentTime();
	}

	return pInfo;
}

void InterceptInfo::AnalyseInterceptSolution(const BallState & ball, PlayerInterceptInfo *pInfo)
{
	pInfo->mIntervals = (pInfo->solution.interc == 1)? 1: 2;
	pInfo->mMinCycle = pInfo->mInterCycle[0];
	if (pInfo->mMinCycle == 0) pInfo->mIntervals = 0; //表示可踢
	pInfo->mInterPos = ball.GetPredictedPos(pInfo->mMinCycle);

	if (!ServerParam::instance().pitchRectanglar().IsWithin(pInfo->mInterPos, 4.0)){
		pInfo->mRes = IR_Failure;
	}
	else {
		pInfo->mRes = IR_Success;
	}
}

void InterceptInfo::CalcIdealInterception(const BallState & ball, PlayerInterceptInfo *pInfo, const double & buffer)
{
	const int idle_cycle = pInfo->mpPlayer->GetIdleCycle();

	//step 1. 求解简化截球模型
	InterceptModel::instance().CalcInterception(ball.GetPredictedPos(idle_cycle), ball.GetPredictedVel(idle_cycle), buffer, pInfo->mpPlayer, &(pInfo->solution));

	//step 2. 修正截球区间
	//	取整
	if (pInfo->solution.interc == 1){
		pInfo->mInterCycle[0] = (int)floor(pInfo->solution.intert[0]);
	}
	else { //3
		pInfo->mInterCycle[0] = (int)floor(pInfo->solution.intert[0]);
		pInfo->mInterCycle[1] = (int)ceil(pInfo->solution.intert[1]);
		pInfo->mInterCycle[2] = (int)floor(pInfo->solution.intert[2]);


		if (pInfo->mInterCycle[0] > pInfo->mInterCycle[1]){
			pInfo->mInterCycle[0] = pInfo->mInterCycle[2];
			pInfo->solution.interc = 1;
		}
	}

	bool kickable = pInfo->mpPlayer->GetPos().Dist(ball.GetPredictedPos(idle_cycle)) < buffer; //判断开始截球的第一个周期，是否可踢
	if (!kickable) {
		pInfo->mInterCycle[0] = Max(pInfo->mInterCycle[0], 1);
	}
	else {
		pInfo->mInterCycle[0] = 0;
	}

	if (pInfo->solution.interc > 1){
		pInfo->mInterCycle[1] = Max(pInfo->mInterCycle[1], pInfo->mInterCycle[0]);
		pInfo->mInterCycle[2] = Max(pInfo->mInterCycle[2], pInfo->mInterCycle[1]);
	}

	if (pInfo->solution.interc <= 1){
		pInfo->mInterCycle[0] += idle_cycle;
	}
	else {
		pInfo->mInterCycle[0] += idle_cycle;
		pInfo->mInterCycle[1] += idle_cycle;
		pInfo->mInterCycle[2] += idle_cycle;
	}
}

void InterceptInfo::CalcLooseInterception(const BallState & ball, PlayerInterceptInfo *pInfo, const double & buffer)
{
	CalcIdealInterception(ball, pInfo, buffer);
	AnalyseInterceptSolution(ball, pInfo);
}

void InterceptInfo::CalcTightInterception(const BallState & ball, PlayerInterceptInfo *pInfo, bool can_inverse)
{
	CalcIdealInterception(ball, pInfo, pInfo->mpPlayer->GetKickableArea()); //TODO: 改成从外面传进来 buffer

	const int idle_cycle = pInfo->mpPlayer->GetIdleCycle();

	//根据go_to_point模型修正
	if (pInfo->solution.interc == 1){
		int cycle_sup = pInfo->mInterCycle[0];
		int cycle_inf = MobileState::Predictor::MAX_STEP;
		for (int i = cycle_sup; i <= cycle_inf; ++i){
			int n = Dasher::instance().CycleNeedToPoint(*(pInfo->mpPlayer), ball.GetPredictedPos(i), can_inverse) + idle_cycle;
			if (n <= i){ //n以后完全可截
				break;
			}
			pInfo->mInterCycle[0] ++;
		}
	}
	else { //3
		int cycle_sup = pInfo->mInterCycle[0];
		int cycle_inf = pInfo->mInterCycle[1];
		for (int i = cycle_sup; i <= cycle_inf; ++i){
			int n = Dasher::instance().CycleNeedToPoint(*(pInfo->mpPlayer), ball.GetPredictedPos(i), can_inverse) + idle_cycle;
			if (n <= i){
				break;
			}
			pInfo->mInterCycle[0] ++;
		}
		if (pInfo->mInterCycle[0] <= pInfo->mInterCycle[1]){
			cycle_sup = pInfo->mInterCycle[0];
			cycle_inf = pInfo->mInterCycle[1];
			for (int i = cycle_inf; i >= cycle_sup; --i){
				int n = Dasher::instance().CycleNeedToPoint(*(pInfo->mpPlayer), ball.GetPredictedPos(i), can_inverse) + idle_cycle;
				if (n <= i){
					break;
				}
				pInfo->mInterCycle[1] --;
			}
			if (pInfo->mInterCycle[0] > pInfo->mInterCycle[1]){ //窗口收缩成点
				pInfo->solution.interc = 1;
			}
		}
		else {
			pInfo->solution.interc = 1;
		}

		cycle_sup = pInfo->mInterCycle[2];
		cycle_inf = MobileState::Predictor::MAX_STEP;
		for (int i = cycle_sup; i <= cycle_inf; ++i){
			int n = Dasher::instance().CycleNeedToPoint(*(pInfo->mpPlayer), ball.GetPredictedPos(i), can_inverse) + idle_cycle;
			if (n <= i){ //n以后完全可截
				break;
			}
			pInfo->mInterCycle[2] ++;
		}

		if (pInfo->solution.interc == 1){
			pInfo->mInterCycle[0] = pInfo->mInterCycle[2];
		}
	}

	AnalyseInterceptSolution(ball, pInfo);
}

PlayerInterceptInfo *InterceptInfo::GetPlayerInterceptInfo(Unum unum) const
{
	if (!mpWorldState->GetPlayer(unum).IsAlive()) { return 0; }

	PlayerInterceptInfo *pInfo = const_cast<PlayerInterceptInfo *>(unum > 0? &(mTeammateInterceptInfo[unum]): &(mOpponentInterceptInfo[-unum]));
	pInfo->mpPlayer = & (mpWorldState->GetPlayer(unum));
	return pInfo;
}
