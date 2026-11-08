#include "Combat/GirlSaviorFireSchedule.h"
#if defined(GIRLSAVIOR_SCHEDULE_STANDALONE)
#include <cstdio>
int main()
{
 int Failures = 0;
 auto Check = [&](bool Pass, const char* Label) { std::printf("%s %s\n", Pass ? "PASS" : "FAIL", Label); Failures += !Pass; };
#else
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGirlSaviorFireScheduleTest, "GirlSavior.Combat.FireSchedule",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGirlSaviorFireScheduleTest::RunTest(const FString& Parameters)
{
 auto Check = [&](bool Pass, const char* Label) { TestTrue(UTF8_TO_TCHAR(Label), Pass); };
#endif
#endif
#if defined(GIRLSAVIOR_SCHEDULE_STANDALONE) || WITH_DEV_AUTOMATION_TESTS
 FGirlSaviorFireSchedule S;
 S.Press(0.); S.Release();
 Check(S.ConsumeDue(0.), "ready short tap survives release");
 S.Launched(0., 4.);
 Check(!S.ConsumeDue(.25), "released tap never repeats");
 S.Press(.1); S.Release();
 Check(!S.ConsumeDue(.1), "cooldown tap discarded");
 Check(!S.ConsumeDue(.25), "cooldown tap is not buffered");
 S.Press(.2);
 Check(!S.ConsumeDue(.249), "held input cannot bypass deadline");
 Check(S.ConsumeDue(.25), "held input fires at deadline");
 S.Launched(.25, 4.);
 S.Release(); S.Press(.3); S.Release(); S.Press(.31);
 Check(!S.ConsumeDue(.49), "ordered release press retains cooldown");
 Check(S.ConsumeDue(.5), "new held press crosses cooldown");
 S.Launched(.5, 8.);
 Check(!S.ConsumeDue(.624), "new rate applies after launch");
 Check(S.ConsumeDue(.625), "eight per second sets .125 interval");
 S.Launched(.625, 4.); S.Cancel();
 Check(!S.ConsumeDue(1.), "cancel requires new press");
 S.Press(.7); S.Release();
 Check(!S.ConsumeDue(.875), "cancel preserves deadline and discards early tap");
 S.Press(1.); S.Cancel();
 Check(!S.ConsumeDue(1.), "cancel clears ready press as well as held");
 S.Press(1.);
 Check(S.ConsumeDue(1.), "fresh press rearms");
 S.Launched(1.,4.);
 Check(S.ConsumeDue(3.), "hitch resumes held fire");
 S.Launched(3.,4.);
 Check(!S.ConsumeDue(3.) && !S.ConsumeDue(3.249), "hitch never produces catch-up burst");
 Check(S.ConsumeDue(3.25), "next interval starts at actual resumed shot");
 S.Launched(3.25,4.); S.Release(); S.Press(3.3); S.Release();
 Check(!S.ConsumeDue(3.5), "short press readiness measured at edge rather than tick");
#if defined(GIRLSAVIOR_SCHEDULE_STANDALONE)
 return Failures ? 1 : 0;
#else
 return true;
#endif
}
#endif
