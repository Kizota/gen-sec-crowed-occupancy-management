#include "DividerManager.hpp"

bool IsLeader(Divider divider)
{
    return divider.IsLeader();
}

DividerManager::DividerManager(int id) : id(id), metaAliveTracker(20, this), sender(nullptr), localCollector(nullptr), role(), trafficState(IDLE_T)
{
    timer.SetInterval(WAIT_INTERVAL);
}

bool DividerManager::SetSender(ISender *newSender)
{
    if (newSender == nullptr)
    {
        return false;
    }

    this->sender = newSender;
    return true;
}

bool DividerManager::SetLocalCollector(ILocalCollector *newLocalCollector)
{
    if (newLocalCollector == nullptr)
    {
        return false;
    }

    this->localCollector = newLocalCollector;

    return true;
}

std::pair<int, int> DividerManager::GetLeastBusyGate()
{
    // least busy gate initialization
    std::pair<int, int> leastBusyGate;
    leastBusyGate.first = -1;   // id
    leastBusyGate.second = 100; // busy rate

    for (auto &divider : dividers)
    {
        std::pair<int, int> checkedGate = divider.GetGateData();
        // Serial.println("rem");
        if (checkedGate.second < leastBusyGate.second && divider.IsAcive())
        // update least busy gate which have the lower busy rate
        {
            leastBusyGate = checkedGate;
        }
    }

    return leastBusyGate;
}

int DividerManager::GetId()
{
    return id;
}

RoleMode DividerManager::GetRoleMode()
{
    return role.GetMode();
}

bool DividerManager::Add(int newId)
{
    // check id is exist
    if (IsMemberExist(newId))
    {
        return false;
    }

    dividers.push_back(Divider(newId, false));

    return true;
}

bool DividerManager::Remove(int removedId)
{
    bool sta = false;

    // search for the divider to remove
    auto divider = std::find(dividers.begin(), dividers.end(), removedId);

    if (divider != dividers.end())
    {
        // case if leader is remove - find another leader
        if (divider->IsLeader())
        {
            role.SetLostLeader();
        }

        Serial.println("divider is removed with id: ");
        Serial.print(removedId);
        dividers.remove(*divider);
        sta = true;
    }

    return sta;
}

void DividerManager::dividersChat()
{
    // RoleMode rolecheck = role.GetMode();
    // Serial.println(rolecheck);
    switch (role.GetMode())
    {
    case IDLE:
        if (role.IsNewMode())
        // send discover message
        {

            sender->SendMessage(DIVIDER_ROLE, id, DISCOVER);

            timer.Reset();
            role.ClearEntryFlag();
        }

        if (role.IsAssignedMember())
        // being assigned to be member by an exist leader
        {
            role.UpdateMode(MEMBER);
        }

        if (timer.isTimeOut())
        // if leader is not exist
        {
            role.UpdateMode(NEUTRAL);
        }
        break;
    case NEUTRAL:
        if (role.IsNewMode())
        // check if become the next leader
        {
            role.ClearEntryFlag();
            if (IsNextLeader())
            {
                Serial.println("being leader!");
                sender->SendMessage(DIVIDER_ROLE, id, NEW_LEADER);
                role.UpdateMode(LEADER);
            }
        }

        if (role.IsAssignedMember())
        // if other is assigned as leader already
        {
            role.UpdateMode(MEMBER);
            sender->SendMessage(DIVIDER_ALIVE, id, RoleControl::RoleToInt(role.GetMode())); // double check? make sure other know it as member.
        }
        break;

    case MEMBER:
        if (role.IsNewMode())
        // start to keep track on the leader hearbeat
        {
            Serial.println("being member!");
            localCollector->SetActivateState(false);

            // destroy all current heart
            metaAliveTracker.RemoveAll();
            // add and start tracking the leader heart
            metaAliveTracker.Add(SearchLeaderId());
            metaAliveTracker.StartTracking(SearchLeaderId());

            // reset the timer period for sending leader heartbeat
            timer.SetInterval(DEFAULT_RATE);
            timer.Reset();

            role.ClearEntryFlag();
        }

        if (timer.isTimeOut())
        // send data heartbeat to leader for every 5 second
        {
            // Serial.println("member alive-------------: ");
            // Serial.println("number of divider: ");
            // / Serial.println(dividers.size());

            sender->SendMessage(DIVIDER_ALIVE, id, SearchLeaderId(), MEMBER_ALIVE, localCollector->GetLeastBusyGate());
            Serial.println(localCollector->GetLeastBusyGate().first);
            Serial.println(localCollector->GetLeastBusyGate().second);

            timer.Reset();
        }

        if (role.IsLostedLeader())
        // when the leader heart is sucide and lost leader flag is turn on
        {
            sender->SendMessage(DIVIDER_ROLE, id, LEADER_DEAD);
            role.UpdateMode(NEUTRAL);
        }

        break;
    case LEADER:
        if (role.IsNewMode())
        // begin the heart beat timer
        {
            Serial.println("activate local gate manager!!!!!!!!!!");
            localCollector->SetActivateState(true);
            Serial.println("being leader!");
            // activate the local divider - gatemanager

            for (auto &divider : dividers)
            // add heart and start tracking all members
            {
                if (!divider.IsLeader())
                // REVIEW - how could the member be a leader error already handle?
                {
                    // turn off all
                    metaAliveTracker.Add(divider.GetId());
                    metaAliveTracker.StartTracking(divider.GetId());

                    // deactivate all member
                    sender->SendMessage(DIVIDER_ROLE, id, divider.GetId(), DEACTIVATE);
                }
            }

            // reset the timer period for sending leader heartbeat
            timer.SetInterval(DEFAULT_RATE);
            timer.Reset();

            role.ClearEntryFlag();
        }

        ControlDividerActivation();

        if (timer.isTimeOut())
        // leader sent heartbeat to notify member of its alive
        {

            sender->SendMessage(DIVIDER_ALIVE, id, LEADER_ALIVE);
            timer.Reset();
        }

        if (role.IsAssignedMember())
        // other higher priority re-assigned the fake leader back to member
        {
            role.UpdateMode(MEMBER);
        }
        break;
    default:
        // doing nothing - handle invalid role mode
        break;
    }
}

// REVIEW - when fellow member and leader is sent
//  handle message: FELLOW_MEMBER + FELLOW_LEADER
void DividerManager::HandleDiscoverResult(int dividerId, RoleMode dividerRole)
{
    // add a new divider id
    bool addSta = Add(dividerId);

    // set the role of new divider id
    bool setRoleSta = SetDividerRole(dividerId, dividerRole);

    // become a member if there is a leader response
    if (dividerRole == LEADER)
    {
        role.SetAssignedMember();
        sender->SendMessage(DIVIDER_ROLE, id, 222);
    }
}

void DividerManager::HandleNewMember(int newId)
{
    if (JustifyMember(newId))
    // check if the new divider has the same id
    {
        Serial.println("accept new member ");
        Add(newId);
        // if (Add(newId))
        // {
        sender->SendMessage(DIVIDER_ROLE, id, newId, RoleControl::RoleToInt(role.GetMode())); // introduce its role to new members

        if (role.GetMode() == LEADER)
        // start to keep track on new member of currently is a leader
        {
            metaAliveTracker.Add(newId);
            metaAliveTracker.StartTracking(newId);
        }
        // }
        // else
        // // REVIEW - if not add successfully => divider id is already exist
        // {

        // }
    }
}

void DividerManager::HandleNewLeader(int dividerId)
{
    if (JustifyLeader(dividerId))
    // accept new leader & become member!
    {
        SetDividerRole(dividerId, LEADER);
        role.SetAssignedMember();
    }
    else
    // no! get back to be a member!
    {
        sender->SendMessage(DIVIDER_ROLE, id, dividerId, RoleControl::RoleToInt(role.GetMode()));
    }
}

void DividerManager::HandleLeaderAlive(int leaderId)
{
    switch (role.GetMode())
    {
    case LEADER:
        // handle the case if there is an existence of another leader
        HandleNewLeader(leaderId);
        break;

    case MEMBER:
        if (leaderId == SearchLeaderId())
        // read
        {
            Serial.println("update leader beat");
            metaAliveTracker.UpdateNewBeat(leaderId);
        }
        break;
    }
}

void DividerManager::HandleMemberAlive(int memId, std::pair<int, int> data)
{
    if (role.GetMode() == MEMBER)
    // cancel reading the member heartbeat if currently is the leader
    {
        return;
    }

    // finding the divider with the corresponding id
    auto divider = std::find(dividers.begin(), dividers.end(), memId);

    if (divider != dividers.end())
    // update the latest least busy gate and add heartbeat id from remote divider
    {
        Serial.println("update member beat!!");
        divider->UpdateLeastBusyGate(data);
        metaAliveTracker.UpdateNewBeat(memId);
    }
    else
    // handle new member if did not know this member yet
    {
        HandleNewMember(memId);
    }
}

void DividerManager::HandleActivateCommand(bool sta)
{
    localCollector->SetActivateState(sta);
}

bool DividerManager::JustifyMember(int memberId)
{
    return id != memberId;
}

bool DividerManager::JustifyLeader(int leaderId)
{
    bool accept = true;

    if (role.GetMode() == LEADER && id < leaderId)
    {
        if (id < leaderId)
        {
            accept = false;
        }
    }

    return accept;
}

// TODO: check how lampda chan be used properly
int DividerManager::IsNextLeader()
{
    bool isNextLeader = true;

    int checkId = this->id;

    auto IsLowerPriority = [&checkId](Divider &divider)
    {
        return checkId > divider.GetId();
    };

    // check if there is any other id has higher priority
    auto divider = std::find_if(dividers.begin(), dividers.end(), IsLowerPriority);

    if (divider != dividers.end())
    {
        isNextLeader = false;
    }

    return isNextLeader;
}

int DividerManager::SearchLeaderId()
{
    for (auto &divider : dividers)
    {
        if (divider.IsLeader())
        {
            return divider.GetId();
        }
    }

    return -1;
}

int DividerManager::GetActiveDivider()
{
    int numOfActive = 0;

    for (auto &divider : dividers)
    {
        if (divider.IsAcive())
        {
            numOfActive++;
        }
    }

    return numOfActive;
}

bool DividerManager::IsMemberExist(int checkId)
{
    auto divider = std::find(dividers.begin(), dividers.end(), checkId);

    return divider != dividers.end();
}

bool DividerManager::SetDividerRole(int id, RoleMode role)
{
    bool sta = false;
    // find the divider with the corresponding id
    auto divider = std::find(dividers.begin(), dividers.end(), id);

    // check if divider's id is found
    if (divider != dividers.end())
    {
        divider->SetLeader(role);
        sta = true;
    }

    return sta;
}

int DividerManager::GetGeneralBusyRate()
{
    int numOfActiveDivider = 0;
    int sum = 0;

    for (auto &divider : dividers)
    {
        if (divider.IsAcive())
        // only calculation based on the information of active divider
        {
            sum += divider.GetGateData().second;
            numOfActiveDivider++;
        }
    }
    static int preBusyRate = 100;

    int busyRate = 100;
    if (numOfActiveDivider != 0)
    {
        busyRate = (int)(((float)sum / (100.0 * numOfActiveDivider)) * 100.0);
        // calcuate the general busy rate
    }

    if (preBusyRate != busyRate)
    {
        Serial.println("General busy rate ----:");
        Serial.println(busyRate);
        preBusyRate = busyRate;
    }

    return busyRate;
}

int DividerManager::ActivateADivider()
{
    for (auto &divider : dividers)
    {
        if (!divider.IsAcive())
        {
            divider.SetActiveState(true);
            Serial.println("ACTIVATE NEW DIVIDDER SUCCESSFULY!");

            return divider.GetId();
        }
    }

    return -1;
}

int DividerManager::DeactivateADivider()
{
    for (auto &divider : dividers)
    {
        if (divider.IsAcive())
        {
            divider.SetActiveState(false);
            return divider.GetId();
        }
    }

    return -1;
}

void DividerManager::ControlDividerActivation()
{
    static TrafficState preState = CROWD;
    static int preSize = 0;
    switch (trafficState)
    {

    case IDLE_T:
        if (preState != trafficState)
        {
            Serial.println("DIVIDER STATE: IDLE_T");
            preState = trafficState;
        }

        if (preSize != dividers.size())
        {
            preSize = dividers.size();
            Serial.print("number of member divider: ");
            Serial.println(preSize);
        }

        if (dividers.size() > 0 && localCollector->IsBusy())
        // only start the logic when the current gate is too busy
        {
            for (auto &divider : dividers)
            {
                divider.SetActiveState(false);
                sender->SendMessage(DIVIDER_ROLE, id, divider.GetId(), DEACTIVATE);
                Serial.print("send deactivate to divider: ");
                Serial.println(divider.GetId());
            }
            trafficState = NORMAL;
        }

        break;
    case NORMAL:
    {
        if (preState != trafficState)
        {
            Serial.println("DIVIDER STATE: NORMAL");
            preState = trafficState;
        }

        if (dividers.size() == 0 && localCollector->IsFree())
        // go back to idle if normal is not busy anymore
        {
            // TODO: turn off all divider
            trafficState = IDLE_T;
        }

        int busyRate = GetGeneralBusyRate();

        if (busyRate > 80 && localCollector->GetBusyRate() > 80)
        {
            trafficState = CROWD;
        }

        else if (busyRate < 30 && localCollector->GetBusyRate() < 30)
        // only close more gate when main divider is less busy
        {
            trafficState = UNOCCUPIED;
        }

        break;
    }
    case CROWD:
        // REVIEW - it will get stuck at the crowd if still needed for divider active
        {
            if (preState != trafficState)
            {
                Serial.println("DIVIDER STATE: CROWD");
                preState = trafficState;
            }
            // search for a divider to activate
            int activatedId = ActivateADivider();

            if (activatedId != -1)
            // send message to activate the divider if can find more divider is available for activate
            {
                sender->SendMessage(DIVIDER_ROLE, id, activatedId, ACTIVATE);
            }

            if (GetGeneralBusyRate() < 80)
            // stay activate more divider until the busy rate of all least busy gate smaller than 50%
            {
                trafficState = NORMAL;
            }
        }
        break;
    case UNOCCUPIED:
    {
        if (preState != trafficState)
        {
            Serial.println("DIVIDER STATE: UNOCCUPIED");
            preState = trafficState;
        }
        // search for a divider to deactivate
        int deactivateId = DeactivateADivider();

        if (deactivateId != -1)
        // send message to deactivate the divide if can find more divider is available for deactivate
        {
            sender->SendMessage(DIVIDER_ROLE, id, deactivateId, DEACTIVATE);
        }

        if (GetGeneralBusyRate() > 30)
        // staty deactivate more divider until the busy rate of all least busy gate larger than 20%
        {
            trafficState = NORMAL;
        }

        break;
    }
    }
}
